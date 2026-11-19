"""Exercise failure handling against a real DayZ compile error in an empty world.

Run explicitly with DayZ and its server closed. The deliberately invalid mission
never initializes; no existing world or save is opened.
"""
from pathlib import Path
import json
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import platform_win
import world
from session_store import atomic_json


def main():
    if platform_win.find_processes("DayZServer_x64.exe") or platform_win.find_processes("DayZ_x64.exe"):
        raise RuntimeError("Close the existing game/server before running the failure test")
    profile = "guard-compile-" + uuid.uuid4().hex[:6]
    world.select_profile(profile)
    world.setup()
    baseline = ROOT / "mods/@PredateBaseline"
    if baseline.is_dir():
        world.MODS[-1] = baseline
    (world.MISSION / "init.c").write_text(
        "void main() { PredateGuardCompileFixtureMissing(); }\n"
        "Mission CreateCustomMission(string path) { return new MissionServer; }\n")
    try:
        world.start()
    except RuntimeError:
        identity = json.loads(world.STATE.read_text())
        failure = world.process_failure(identity)
        deadline = time.monotonic() + 3
        while failure and "closed_at" not in failure and time.monotonic() < deadline:
            time.sleep(.05)
            failure = world.process_failure(identity)
        if not failure or failure["kind"] != "compile":
            raise AssertionError("The actual compile failure was not recorded")
        if world.running():
            raise AssertionError("The failed server remains open")
        elapsed = failure["closed_at"] - failure["detected_at"]
        if elapsed >= 1:
            raise AssertionError(f"Failure took {elapsed:.3f}s to close")
        report = {"passed": True, "profile": profile, "kind": failure["kind"],
                  "message": failure["message"], "detection_to_close_ms": round(elapsed * 1000, 2)}
        atomic_json(ROOT / "build/failure-guard-result.json", report)
        print(json.dumps(report, indent=2))
        return
    raise AssertionError("The deliberately invalid mission unexpectedly initialized")


if __name__ == "__main__":
    main()
