"""Packaged launcher backend and line-delimited JSON bridge."""
from contextlib import redirect_stdout
import json
import os
from pathlib import Path
import sys
import traceback

from app_runtime import ROOT
import session
import session_store as store
import world
import loadouts
import maps
import dlc_access
import operation_setup
import spawn_points
import workshop_lasers


def initialize():
    """Import the protected original as a separate operation, once."""
    prefs = store.preferences()
    if prefs["selected_world"] != "main" or not world.stable_fallback():
        return
    record_path = session.RUNTIME / "original-import.json"
    record = session.read_native(record_path)
    profile = record.get("profile")
    if profile and (store.world_path(profile) / "operation.json").exists():
        store.save_preferences({"selected_world": profile})
        return
    snapshot = store.snapshot("main", reason="before-launcher-upgrade")
    operation = store.restore_snapshot(snapshot.name, "Current Hunt")
    store.atomic_json(record_path, {"schema": 1, "profile": operation["id"], "backup": snapshot.name})
    store.save_preferences({"selected_world": operation["id"]})


def map_options():
    available = dlc_access.availability()
    return [{"id": item.id, "name": item.name, "size": item.size, "available": available[item.id],
             "spawnPoints": spawn_points.options(item.id)} for item in maps.TERRAINS.values()]


def overview():
    operations = store.worlds()
    preferences = store.preferences()
    imported = session.read_native(session.RUNTIME / "original-import.json").get("profile")
    operations = [
        {**item, "name": "Current Hunt"}
        if item["id"] == imported and item["name"] == "Current operation" else item
        for item in operations
    ]
    operation_options = {"opponents": store.OPPONENT_COUNTS, "randomRegion": True}
    if workshop_lasers.preview_available(ROOT, loadouts.WORKSHOP):
        operation_options["weaponModule"] = {"className": "MM_ANPEQ15", "name": "AN/PEQ-15 Laser / Light Module"}
    return {"operations": operations, "preferences": preferences, "callsign": store.callsign(),
            "loadouts": loadouts.options(preferences["extra_mods"]),
            "spawnPoints": spawn_points.options(),
            "maps": map_options(),
            "operationOptions": operation_options,
            "backups": [item for item in store.backups() if item.get("kind", "operation") == "operation" and not item["file"].startswith("crashlab-")],
            "status": session.state()}


def dispatch(method, params):
    if method == "initialize":
        initialize()
        return overview()
    if method == "overview": return overview()
    if method == "status": return session.state()
    if method == "profile-stats": return store.profile_stats()
    if method == "save-callsign":
        if not isinstance(params, dict) or set(params) != {"callsign"}:
            raise ValueError("Enter a callsign.")
        return store.save_callsign(params["callsign"])
    if method == "scores":
        profile = params.get("profile")
        if not isinstance(profile, str):
            raise ValueError("Select a save to view scores.")
        path = store.existing_world_path(profile)
        return {"name": store.display_name(profile), "lives": list(store.life_scores(path / "profile"))}
    if method == "preferences":
        allowed = {"reconnect_grace", "backup_limit", "offline", "extra_mods", "receiver_scale", "receiver_high_contrast"}
        if not isinstance(params, dict) or set(params) - allowed:
            raise ValueError("Unsupported settings")
        if "offline" in params and not isinstance(params["offline"], bool):
            raise ValueError("Offline must be true or false")
        return store.save_preferences(params)
    if method == "action":
        allowed = {"action", "profile", "name", "backup", "loadout", "opponents", "footprints", "spawn", "setup", "map", "diagnostics"}
        if not isinstance(params, dict) or set(params) - allowed: raise ValueError("Unsupported action parameters")
        values = dict(params)
        action = values.pop("action", None)
        if action not in {"continue", "quit", "new", "restore", "backup", "delete"}: raise ValueError("Unsupported action")
        selected_map = maps.DEFAULT_MAP
        if "map" in values:
            if action != "new": raise ValueError("Choose a map when creating a new hunt.")
            selected_map = maps.terrain(values["map"]).id
        if action == "new": dlc_access.require_files(selected_map)
        if action == "delete" and (set(values) != {"profile"} or not isinstance(values["profile"], str)):
            raise ValueError("Select a save to delete.")
        if 'loadout' in values:
            if action != 'new': raise ValueError('A starting kit applies only to a new operation')
            loadouts.get(values['loadout'])
        if 'opponents' in values:
            if action != 'new': raise ValueError('Opponent count applies only to a new operation')
            store.validate_opponent_count(values['opponents'])
        if 'footprints' in values:
            if action != 'new': raise ValueError('Footprint clues apply only to a new operation')
            if not isinstance(values['footprints'], bool): raise ValueError('Footprint clues must be true or false')
        if 'spawn' in values:
            if action != 'new': raise ValueError('Spawn selection applies only to a new operation')
            spawn_points.validate(values['spawn'], selected_map)
        if 'setup' in values:
            if action != 'new': raise ValueError('Game settings apply only to a new operation')
            operation_setup.config(values['setup'])
        if "profile" in values: store.world_path(values["profile"])
        if "backup" in values and Path(values["backup"]).name != values["backup"]: raise ValueError("Invalid backup")
        if "diagnostics" in values and (action not in {"continue", "new"} or type(values["diagnostics"]) is not bool):
            raise ValueError("Server diagnostics apply only when opening a hunt")
        launcher_pid = os.environ.get("PREDATE_LAUNCHER_PID", "")
        if action in {"continue", "new"} and launcher_pid.isdecimal():
            values["launcher_pid"] = int(launcher_pid)
        return {"requestId": session.request(action, **values)}
    if method == "result":
        identifier = params.get("requestId", "")
        if len(identifier) != 32 or any(c not in "0123456789abcdef" for c in identifier): raise ValueError("Invalid request")
        return session.read_native(session.RUNTIME / "results" / (identifier + ".json")) or None
    raise ValueError("Unknown backend method")


def bridge():
    for line in sys.stdin:
        request = {}
        try:
            if len(line) > 1_000_000: raise ValueError("Request is too large")
            request = json.loads(line)
            with redirect_stdout(sys.stderr):
                result = dispatch(request["method"], request.get("params", {}))
            response = {"id": request["id"], "ok": True, "result": result}
        except Exception as error:
            traceback.print_exc(file=sys.stderr)
            response = {"id": request.get("id"), "ok": False, "error": str(error)}
        print(json.dumps(response), flush=True)


def main():
    mode = sys.argv.pop(1) if len(sys.argv) > 1 else "bridge"
    if mode == "bridge": bridge()
    elif mode == "session": session.main()
    elif mode == "process_guard":
        import process_guard
        process_guard.main()
    else: raise ValueError("Unknown backend mode")


if __name__ == "__main__": main()
