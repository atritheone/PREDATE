"""Keep score snapshots for each play session, including continued deployments."""
from datetime import datetime, timezone
import json
from pathlib import Path
import zipfile

from lifetime_play_time import read_checkpoint
from score_statistics import count, measurement


HISTORY = "PredateScoreSessions.json"
WORLD_STATE = "profile/PredateWorldState.json"
ARCHIVE_ERRORS = (OSError, ValueError, KeyError, TypeError, AttributeError, zipfile.BadZipFile)


def session_time(value):
    if not isinstance(value, str):
        return None
    try:
        stamp = datetime.fromisoformat(value)
    except ValueError:
        return None
    return stamp.astimezone(timezone.utc).isoformat() if stamp.tzinfo else None


def ordered_lives(records):
    return sorted((life for life in records if count(life, "Life") > 0), key=lambda life: life["Life"])


def make_session(identifier, started_at, opening, closing, lives):
    played = measurement(closing, "SimulationSeconds") - measurement(opening, "SimulationSeconds")
    if not started_at or played <= 0 or not count(closing, "PlayerLife"):
        return None
    first = max(1, count(opening, "PlayerLife"))
    last = count(closing, "PlayerLife")
    deployments = [life for life in ordered_lives(lives) if first <= life["Life"] <= last]
    return {"id": identifier, "startedAt": started_at, "playedSeconds": played, "lives": deployments}


def opening_checkpoint(path, profile, identifier):
    with zipfile.ZipFile(path) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        if (manifest["operation"]["id"] != profile or manifest["reason"] != "before-continue"
                or manifest.get("session_id", "") not in ("", identifier)):
            raise ValueError("Snapshot belongs to a different session")
        return session_time(manifest.get("created")), read_checkpoint(archive, manifest["files"], WORLD_STATE)


def recover_sessions(folder, profile):
    """Pair retained launch and clean close archives before backup rotation removes them."""
    checkpoints = []
    for path in folder.glob(f"{profile}-session-*.zip"):
        try:
            with zipfile.ZipFile(path) as archive:
                manifest = json.loads(archive.read("manifest.json"))
                started_at = session_time(manifest.get("created"))
                if started_at and manifest["operation"]["id"] == profile:
                    checkpoints.append((started_at, path, manifest))
        except ARCHIVE_ERRORS:
            continue
    opening = None
    sessions = {}
    for stamp, path, manifest in sorted(checkpoints):
        reason = manifest.get("reason")
        if reason == "manual":
            continue
        if reason == "before-continue":
            opening = path
            continue
        previous, opening = opening, None
        identifier = manifest.get("session_id")
        if reason != "save-and-quit" or not previous or not isinstance(identifier, str) or not identifier:
            continue
        try:
            started_at, initial = opening_checkpoint(previous, profile, identifier)
            files = manifest["files"]
            with zipfile.ZipFile(path) as archive:
                closing = read_checkpoint(archive, files, WORLD_STATE)
                lives = (read_checkpoint(archive, files, name) for name in files
                         if name.startswith("profile/PredateLifeScore-") and name.endswith(".json"))
                saved = make_session(identifier, started_at, initial, closing, lives)
            if saved:
                sessions[identifier] = saved
        except ARCHIVE_ERRORS:
            continue
    return list(sessions.values())


def load(profile_dir):
    try:
        saved = json.loads((profile_dir / HISTORY).read_text(encoding="utf-8-sig"))
        return saved["sessions"] if saved["schema"] == 1 and isinstance(saved["sessions"], list) else None
    except (OSError, ValueError, KeyError, TypeError):
        return None


def group(folder, profile, profile_dir, records):
    sessions = load(profile_dir)
    if sessions is None:
        sessions = recover_sessions(folder, profile)
    represented = {life["Life"] for session in sessions for life in session["lives"]}
    earlier = [life for life in ordered_lives(records) if life["Life"] not in represented]
    if earlier:
        return [{"id": None, "startedAt": None, "playedSeconds": None, "lives": earlier}, *sessions]
    return sessions


def remember_history(folder, profile, profile_dir, atomic_json):
    if load(profile_dir) is None:
        sessions = recover_sessions(folder, profile)
        if sessions:
            atomic_json(profile_dir / HISTORY, {"schema": 1, "sessions": sessions})


def close_session(folder, profile, profile_dir, pending, native, records, atomic_json):
    identifier = native.get("SessionId")
    backup = pending.get("backup")
    if not identifier or pending.get("clean") or identifier != pending.get("session_id"):
        return
    if not isinstance(backup, str) or not backup or Path(backup).name != backup:
        return
    started_at, initial = opening_checkpoint(folder / backup, profile, identifier)
    saved = make_session(identifier, started_at, initial, native, records)
    if saved is None:
        return
    sessions = load(profile_dir)
    if sessions is None:
        sessions = recover_sessions(folder, profile)
    sessions = [session for session in sessions if session["id"] != identifier]
    sessions.append(saved)
    atomic_json(profile_dir / HISTORY, {"schema": 1, "sessions": sessions})
