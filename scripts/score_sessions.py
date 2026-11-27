"""Group deployments by the session in which they began."""
from datetime import datetime
import json
import zipfile

from lifetime_play_time import read_checkpoint


def session_time(value):
    if not isinstance(value, str):
        return None
    try:
        stamp = datetime.fromisoformat(value)
    except ValueError:
        return None
    return stamp.isoformat() if stamp.tzinfo else None


def legacy_sessions(folder, profile, lives):
    """Use paired launch and closing checkpoints when the saved life matches."""
    remaining = {life["Life"]: life for life in lives if not life.get("SessionId")}
    assignments = {}
    opening = None
    for path in sorted(folder.glob(f"{profile}-session-*.zip")):
        if not remaining:
            break
        try:
            with zipfile.ZipFile(path) as archive:
                manifest = json.loads(archive.read("manifest.json"))
                if manifest["operation"]["id"] != profile or manifest["reason"] == "manual":
                    continue
                files = manifest["files"]
                state = read_checkpoint(archive, files, "profile/PredateWorldState.json")
                if manifest["reason"] == "before-continue":
                    first = state.get("PlayerLife", 1)
                    elapsed = state.get("SimulationSeconds", 0)
                    if elapsed or first > 1 or "profile/PredateLifeScore-1.json" in files:
                        first += 1
                    opening = (first, session_time(manifest.get("created")), manifest.get("session_id"))
                    continue
                if not opening:
                    continue
                first, started_at, opening_id = opening
                opening = None
                identifier = manifest.get("session_id")
                if not isinstance(identifier, str) or not identifier or (opening_id and opening_id != identifier):
                    continue
                last = state.get("PlayerLife", 0)
                for number, life in list(remaining.items()):
                    if first <= number <= last:
                        saved = read_checkpoint(archive, files, f"profile/PredateLifeScore-{number}.json")
                        if saved.get("Life") == number and saved.get("StartedAt") == life.get("StartedAt"):
                            assignments[number] = (identifier, started_at)
                            del remaining[number]
        except (OSError, ValueError, KeyError, TypeError, AttributeError, zipfile.BadZipFile):
            opening = None
    return assignments


def group(folder, profile, records):
    lives = sorted((life for life in records if type(life.get("Life")) is int and life["Life"] > 0),
                   key=lambda life: life["Life"])
    legacy = legacy_sessions(folder, profile, lives)
    groups = {}
    for life in lives:
        identifier = life.get("SessionId")
        started_at = session_time(life.get("SessionStartedAt"))
        if not isinstance(identifier, str) or not identifier:
            identifier, started_at = legacy.get(life["Life"], (None, None))
        session = groups.setdefault(identifier, {"id": identifier, "startedAt": started_at, "lives": []})
        if not session["startedAt"] and started_at:
            session["startedAt"] = started_at
        session["lives"].append(life)
    return list(groups.values())
