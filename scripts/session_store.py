"""World metadata, launcher preferences and validated save snapshots."""
from __future__ import annotations
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sqlite3
import tempfile
import time
import uuid
import zipfile
import mod_presets
import loadouts
import spawn_points
import presentation
import release
import workshop_lasers
from contextlib import closing

from app_runtime import ROOT
SCHEMA = 1
DEFAULTS = {"schema": SCHEMA, "selected_world": "main", "reconnect_grace": 30,
            "offline": False, "extra_mods": [], "backup_limit": 20,
            "receiver_scale": 100, "receiver_high_contrast": False}


def atomic_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    try:
        temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
        # Windows readers and sync/indexing tools can briefly deny replacement
        # even though the destination is writable. Retain the old complete file
        # and retry the same complete temporary file for a bounded interval.
        for attempt in range(6):
            try:
                os.replace(temporary, path)
                break
            except OSError as error:
                transient = isinstance(error, PermissionError) or getattr(error, 'winerror', None) in (5, 32, 33)
                if not transient or attempt == 5: raise
                time.sleep(0.01 * (2 ** attempt))
    finally:
        temporary.unlink(missing_ok=True)


def read_json(path, default=None):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except FileNotFoundError:
        return default


def preferences():
    saved = read_json(ROOT / "config/launcher-settings.local.json", {})
    if saved.get("schema", 1) != SCHEMA:
        raise ValueError("Unsupported launcher settings version")
    result = DEFAULTS | saved
    result["extra_mods"] = mod_presets.optional_paths(result["extra_mods"])
    return result


def validate_mods(values):
    mod_presets.validate_paths(values)
    core = {(ROOT / "mods" / name).resolve() for name in (*release.DEPENDENCIES, "@Predate")}
    for value in values:
        if Path(value).resolve() in core:
            raise ValueError(f"PREDATE already loads this standard mod: {value}")


def verify_operation_mods(profile):
    info = metadata(profile)
    validate_mods(mod_presets.optional_paths(info['extra_mods']))
    verified = mod_presets.verify(info['extra_mods'], info.get('mod_lock'))
    workshop_lasers.verify_pair(verified)
    if 'mod_lock' not in info:
        info['mod_lock'] = verified
        atomic_json(world_path(profile)/'operation.json', info)
    return verified


def save_preferences(values):
    result = preferences() | values
    result["reconnect_grace"] = int(result["reconnect_grace"])
    if not 0 <= result["reconnect_grace"] <= 120:
        raise ValueError("Reconnect grace must be between 0 and 120 seconds")
    result["backup_limit"] = int(result["backup_limit"])
    if not 1 <= result["backup_limit"] <= 200:
        raise ValueError("Backup retention must be between 1 and 200 snapshots")
    # A missing future-operation preset must not prevent continuing an existing
    # operation that has its own valid, independent preset.
    if "extra_mods" in values:
        result["extra_mods"] = mod_presets.optional_paths(result["extra_mods"])
        validate_mods(result["extra_mods"])
    presentation.validate(result)
    atomic_json(ROOT / "config/launcher-settings.local.json", result)
    return result


def world_path(profile):
    if not re.fullmatch(r"[a-z][a-z0-9-]{0,31}", profile):
        raise ValueError("Invalid world identifier")
    return ROOT / "world" if profile == "main" else ROOT / "worlds" / profile


def metadata(profile):
    path = world_path(profile) / "operation.json"
    saved = read_json(path)
    if saved is not None:
        if saved.get("schema") != SCHEMA: raise ValueError("Unsupported operation metadata")
        return saved
    return {"schema": SCHEMA, "id": profile, "name": "Original Hunt" if profile == "main" else profile,
            "created": None, "extra_mods": [], "kind": "operation" if profile == "main" else "validation"}


def worlds():
    values = [metadata("main")]
    for path in sorted((ROOT / "worlds").glob("*/operation.json")):
        item = read_json(path)
        if item and item.get("kind") == "operation": values.append(item)
    return values


def new_id():
    return "op-" + datetime.now().strftime("%y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:4]


OPPONENT_COUNTS = (1, 3, 6, 15, 30, 50, 100)
MAX_OPPONENT_COUNT = 100
DEFAULT_OPPONENT_COUNT = MAX_OPPONENT_COUNT


def validate_opponent_count(value):
    if type(value) is not int or not 1 <= value <= MAX_OPPONENT_COUNT:
        raise ValueError("Enter a whole number from 1 to 100 opponents")
    return value


def create_operation(name, profile=None, extra_mods=None, hunt_defaults=True, player_loadout='m4a1',
                     opponent_count=1, footprints=False, spawn=None, setup=None, require_modern_gear=False):
    import operation_setup
    import world
    name = name.strip()
    if not name or len(name) > 80: raise ValueError("Use an operation name of 1–80 characters")
    validate_opponent_count(opponent_count)
    if not isinstance(footprints, bool): raise ValueError("Footprint clues must be true or false")
    kit = loadouts.get(player_loadout) if hunt_defaults else None
    player_start = spawn_points.config(spawn) if spawn is not None else {}
    rules = operation_setup.config(setup) if setup is not None else {}
    if kit and kit.get("PlayerPreset") and not rules.get("HasCustomInventory"):
        raise ValueError("Workshop starting kits require a loadout from New Operation")
    mods = preferences()["extra_mods"] if extra_mods is None else mod_presets.optional_paths(extra_mods)
    validate_mods(mods)
    custom_classes = {item["ClassName"] for item in rules.get("PlayerInventory", [])}
    uses_workshop_gear = any(item["id"] in custom_classes for item in loadouts.modern_data()["Catalog"])
    kit_requirements = loadouts.modern_data()["Equipment"] if require_modern_gear or uses_workshop_gear else kit
    if kit_requirements:
        configured = {Path(value).resolve() for value in mods}
        requirements = {"Name": kit["Name"] if kit else "Starting inventory", **kit_requirements}
        packs = loadouts.workshop_paths(requirements, mods)
        mods = [*mods, *(str(pack) for pack in packs if pack.resolve() not in configured)]
    if hunt_defaults:
        configured = {Path(value).resolve() for value in mods}
        pair = workshop_lasers.ready_pair(ROOT, loadouts.WORKSHOP)
        mods = [*mods, *(str(pack) for pack in pair if pack.resolve() not in configured)]
    mod_lock = mod_presets.capture(mods)
    workshop_lasers.verify_pair(mod_lock)
    profile = profile or new_id()
    target = world_path(profile)
    if target.exists(): raise ValueError("That operation already exists; its saved data was retained")
    world.select_profile(profile)
    world.setup()
    if hunt_defaults:
        atomic_json(target / 'profile/PredateConfig.json', {
            'SchemaVersion': 1,
            'EnableFairInformation': True,
            'EnableSharedSignals': True,
            'EnableStrategicHunter': True,
            'PlayerLoadout': player_loadout,
            'RequireModernGear': require_modern_gear,
            'OpponentCount': opponent_count,
            'RandomInitialSpawns': True,
            'EnableFootprints': footprints,
            'EnableOperatorRespawns': True,
            **player_start,
            **rules,
        })
    info = {"schema": SCHEMA, "id": profile, "name": name,
            "created": datetime.now(timezone.utc).isoformat(),
            "extra_mods": mods, "mod_lock": mod_lock, "kind": "operation"}
    atomic_json(target / "operation.json", info)
    return info


def digest(path):
    with Path(path).open("rb") as stream: return hashlib.file_digest(stream, "sha256").hexdigest()


def save_files(profile):
    base = world_path(profile)
    for folder in ("storage", "mission/solo.chernarusplus/storage_1", "profile/ExpansionMod"):
        for path in sorted((base / folder).rglob("*")):
            if path.is_file(): yield path, path.relative_to(base).as_posix()
    for path in sorted((base / "profile").glob("Predate*.json")):
        if path.name in {"PredateSessionStatus.json", "PredateControl.json", "PredateFailure.json"}: continue
        yield path, path.relative_to(base).as_posix()


def snapshot(profile, session_id="", reason="manual"):
    import world
    # Startup has already verified and appended the operation's optional mods.
    # Selecting the same profile again would discard that prepared mod list.
    if world.PROFILE_NAME != profile:
        world.select_profile(profile)
    if world.running(): raise RuntimeError("A save snapshot requires a stopped server")
    folder = ROOT / "backups"
    folder.mkdir(exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    target = folder / f"{profile}-session-{stamp}.zip"
    staging = target.with_suffix(".partial")
    manifest = {"schema": SCHEMA, "operation": metadata(profile), "session_id": session_id,
                "reason": reason, "created": datetime.now(timezone.utc).isoformat(),
                "release": read_json(ROOT / "config/release-lock.json"), "files": {}}
    with zipfile.ZipFile(staging, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        for path, name in save_files(profile):
            payload = path.read_bytes()
            manifest["files"][name] = hashlib.sha256(payload).hexdigest()
            archive.writestr(name, payload)
        archive.writestr("manifest.json", json.dumps(manifest, indent=2))
    validate_snapshot(staging)
    os.replace(staging, target)
    # Retention applies only to this tool's validated session snapshots of this world.
    older = sorted(folder.glob(f"{profile}-session-*.zip"), reverse=True)
    for old in older[preferences()["backup_limit"]:]: old.unlink()
    return target


def validate_snapshot(path):
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None: raise ValueError("Backup ZIP checksum failed")
        names = archive.namelist()
        if len(names) != len(set(names)): raise ValueError("Duplicate backup entries")
        for name in names:
            item = PurePosixPath(name)
            if item.is_absolute() or ".." in item.parts or "\\" in name or ":" in name:
                raise ValueError("Unsafe backup path")
        manifest = json.loads(archive.read("manifest.json"))
        if manifest.get("schema") != SCHEMA: raise ValueError("Unsupported backup schema")
        if set(names) != set(manifest["files"]) | {"manifest.json"}:
            raise ValueError("Backup manifest does not match its files")
        for name, expected in manifest["files"].items():
            payload = archive.read(name)
            if hashlib.sha256(payload).hexdigest() != expected: raise ValueError(f"Backup hash mismatch: {name}")
            if name.endswith("players.db"):
                with tempfile.TemporaryDirectory(prefix="predate-db-") as temporary:
                    database = Path(temporary) / "players.db"
                    database.write_bytes(payload)
                    with closing(sqlite3.connect(database)) as db:
                        if db.execute("PRAGMA quick_check").fetchone()[0] != "ok":
                            raise ValueError("Character database integrity check failed")
        return manifest


def backups():
    result = []
    for path in sorted((ROOT / "backups").glob("*-session-*.zip"), reverse=True):
        try:
            with zipfile.ZipFile(path) as archive: info = json.loads(archive.read("manifest.json"))
            operation = info["operation"]
            kind = operation.get("kind", "operation")
            # Integration checks retire their temporary operation after exercising
            # the real New/Restore actions; keep those snapshots out of normal UI.
            if metadata(operation["id"]).get("kind") == "validation": kind = "validation"
            result.append({"file": path.name, "name": info["operation"]["name"],
                           "created": info["created"], "bytes": path.stat().st_size,
                           "kind": kind})
        except (OSError, ValueError, KeyError, zipfile.BadZipFile):
            result.append({"file": path.name, "name": "Unreadable snapshot", "created": "", "bytes": path.stat().st_size})
    return sorted(result, key=lambda item: item.get('created', ''), reverse=True)


def restore_snapshot(filename, name):
    if Path(filename).name != filename: raise ValueError("Select a backup from the backup list")
    path = ROOT / "backups" / filename
    manifest = validate_snapshot(path)
    current = read_json(ROOT / "config/release-lock.json")
    if not release.snapshot_matches(manifest["release"], current):
        raise ValueError("This snapshot requires a different game/dependency release; its files were retained")
    restored_mod_lock = mod_presets.verify(manifest['operation']['extra_mods'], manifest['operation'].get('mod_lock'))
    info = create_operation(name, extra_mods=manifest["operation"]["extra_mods"], hunt_defaults=False)
    base = world_path(info["id"])
    info["kind"] = "restoring"
    atomic_json(base / "operation.json", info)
    try:
        with zipfile.ZipFile(path) as archive:
            for relative, expected in manifest["files"].items():
                target = base / relative
                if not target.resolve().is_relative_to(base.resolve()): raise ValueError("Unsafe restore path")
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(relative))
                if digest(target) != expected: raise ValueError("Restored file failed verification: " + relative)
    except Exception:
        info["kind"] = "incomplete"
        atomic_json(base / "operation.json", info)
        raise
    info["kind"] = "operation"
    info["extra_mods"] = manifest["operation"]["extra_mods"]
    info['mod_lock'] = restored_mod_lock
    info["restored_from"] = filename
    atomic_json(base / "operation.json", info)
    return info
