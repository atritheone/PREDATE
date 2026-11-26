"""Keep saved play time independently of the folders and snapshots that recorded it."""
from contextlib import closing
import hashlib
import json
import sqlite3
import zipfile

from score_statistics import measurement, played_seconds


def read_checkpoint(archive, files, name):
    expected = files.get(name)
    if not expected:
        return {}
    payload = archive.read(name)
    if hashlib.sha256(payload).hexdigest() != expected:
        raise ValueError("Invalid play time checkpoint")
    record = json.loads(payload)
    return record if isinstance(record, dict) else {}


def read_backup(path):
    try:
        with zipfile.ZipFile(path) as archive:
            manifest = json.loads(archive.read("manifest.json"))
            operation = manifest["operation"]
            files = manifest["files"]
            if not isinstance(operation, dict) or not isinstance(files, dict):
                return None
            state = read_checkpoint(archive, files, "profile/PredateWorldState.json")
            lives = (read_checkpoint(archive, files, name) for name in files
                     if name.startswith("profile/PredateLifeScore-") and name.endswith(".json"))
            return operation, played_seconds(state, (life for life in lives if life.get("Version") == 1))
    except (OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile):
        return None


def inherited_seconds(root, operation, archives):
    if "play_time_offset" in operation:
        return measurement(operation, "play_time_offset")
    filename = operation.get("restored_from")
    if not filename:
        return 0
    if not isinstance(filename, str) or "/" in filename or "\\" in filename:
        return None
    source = archives.get(filename)
    if source is None:
        source = read_backup(root / "backups" / filename)
    return source[1] if source else None


def open_ledger(root):
    path = root / "config/play-time.local.sqlite3"
    path.parent.mkdir(parents=True, exist_ok=True)
    database = sqlite3.connect(path)
    database.execute("CREATE TABLE IF NOT EXISTS play_time (profile TEXT PRIMARY KEY, seconds REAL, inherited REAL)")
    return database


def save_rows(database, rows):
    # Higher checkpoints survive a rollback. A known starting offset survives archive retention.
    database.executemany("""
        INSERT INTO play_time VALUES (?, ?, ?)
        ON CONFLICT(profile) DO UPDATE SET
            seconds = max(play_time.seconds, excluded.seconds),
            inherited = coalesce(excluded.inherited, play_time.inherited)
    """, rows)


def remember(root, operation, seconds):
    if operation.get("kind", "operation") != "operation":
        return
    inherited = inherited_seconds(root, operation, {})
    with closing(open_ledger(root)) as database, database:
        save_rows(database, [(operation["id"], seconds, inherited)])


def total(root, saves, accept_archive):
    with closing(open_ledger(root)) as database:
        archives = {}
        import_history = database.execute("PRAGMA user_version").fetchone()[0] == 0
        if import_history:
            for path in (root / "backups").glob("*-session-*.zip"):
                saved = read_backup(path)
                if saved is not None:
                    archives[path.name] = saved
        history = (record for record in archives.values() if accept_archive(record[0]))
        rows = [(operation["id"], seconds, inherited_seconds(root, operation, archives))
                for records in (history, saves) for operation, seconds in records]
        with database:
            save_rows(database, rows)
            if import_history:
                database.execute("PRAGMA user_version = 1")
            # A legacy copy with an unknown starting point cannot safely add inherited time.
            return database.execute("SELECT coalesce(sum(max(0, seconds - inherited)), 0) FROM play_time").fetchone()[0]
