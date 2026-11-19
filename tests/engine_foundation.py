"""Windows integration checks against an isolated clone of the stopped main world.

Run after scripts/build.py. Replaces ONLY worlds/development's save stores. Leaves main stopped.
All scenarios use the real DayZ server, Expansion storage and PREDATE serializer.
"""
from pathlib import Path
import json
import shutil
import subprocess
import stat
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import world
import release
import platform_win

DEV = ROOT / "worlds/development"
RESULTS = []


def invoke(*args, expect_failure=False):
    result = subprocess.run([sys.executable, str(ROOT / "scripts/world.py"), *args], cwd=ROOT)
    if expect_failure:
        assert result.returncode != 0, "Invalid world was accepted as ready"
    else:
        result.check_returncode()


def reset_clone():
    world.select_profile("development")
    if world.running(): raise RuntimeError("Stop development before replacing its test stores.")
    for rel in ("storage", "mission/solo.chernarusplus/storage_1", "profile/ExpansionMod"):
        target = DEV / rel
        resolved = target.resolve()
        if not resolved.is_relative_to(DEV.resolve()) or resolved == DEV.resolve():
            raise RuntimeError("Unsafe development reset path")
        if target.exists(): shutil.rmtree(target, onexc=clear_readonly)
        original = ROOT / "world" / rel
        if original.exists(): shutil.copytree(original, target)
    for name in ("PredateConfig.json", "PredateProgress.json", "PredateWorldState.json"):
        original = ROOT / "world/profile" / name
        target = DEV / "profile" / name
        if original.exists(): shutil.copy2(original, target)
        else: target.unlink(missing_ok=True)


def clear_readonly(function, path, error):
    # Backup directories can inherit the read-only attribute on Windows.
    if not Path(path).resolve().is_relative_to(DEV.resolve()): raise error
    Path(path).chmod(stat.S_IREAD | stat.S_IWRITE)
    function(path)


def boot(expected, scenario=None):
    before = set((DEV / "profile").glob("script*.log"))
    args = ["start", "--profile", "development"]
    if scenario: args += ["--scenario", scenario]
    invoke(*args, expect_failure=expected.startswith("RECOVERY REQUIRED:"))
    deadline = time.monotonic() + 100
    while time.monotonic() < deadline:
        new = set((DEV / "profile").glob("script*.log")) - before
        content = "\n".join(p.read_text(errors="replace") for p in new)
        if "Virtual Machine Exception" in content or "SCENARIO registry FAIL" in content or "SELFTEST FAIL" in content:
            raise AssertionError(content[-6000:])
        if expected in content:
            RESULTS.append(expected)
            print("PASS:", expected, flush=True)
            return content
        time.sleep(1)
    raise AssertionError("Timed out waiting for " + expected)


def progress(): return json.loads((DEV / "profile/PredateProgress.json").read_text())
def stop(): invoke("stop", "--profile", "development")


def soak(seconds=100):
    """Exercise native repeating timers beyond the short startup checks."""
    world.select_profile("development")
    log = max((DEV / "profile").glob("script*.log"), key=lambda p: p.stat().st_mtime_ns)
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        assert world.running(), "Server exited during the timer soak"
        text = log.read_text(errors="replace")
        assert "Virtual Machine Exception" not in text, text[-6000:]
        time.sleep(2)
    RESULTS.append("100-second native timer soak")
    print("PASS: 100-second native timer soak", flush=True)


def main():
    if platform_win.find_processes("DayZ_x64.exe") or platform_win.find_processes("DayZServer_x64.exe"):
        raise RuntimeError("Close the existing game/server before foundation checks")
    world.select_profile("main")
    if world.running(): raise RuntimeError("Stop main before cloning its stores.")
    live_before = release.fingerprints(ROOT / "world")
    invoke("setup", "--profile", "development")
    try:
        reset_clone()
        legacy = progress()
        boot("SCENARIO registry PASS", "registry")
        soak()
        stop()
        migrated = progress()
        assert migrated["SchemaVersion"] == 1
        assert migrated["Operator"]["Id"] == "raven-01"
        for key in ("Selected", "Defeated", "HasPulse", "LastBearing", "PulseAge", "PulseRemaining", "BotPlacementRevision", "PlayerPlacementRevision"):
            assert migrated[key] == legacy[key], key
        boot("SESSION READY: schema=1; operator=raven-01; state=alive")
        stop()
        assert progress() == migrated, "Headless continuation changed the saved hunt clock"
        boot("RAVEN-01 eliminated; automatic respawn disabled", "defeat")
        stop()
        assert progress()["Defeated"] == 1
        defeated = boot("SELFTEST PASS")
        assert "Restored defeated operator" in defeated
        assert "Spawned registered operator" not in defeated
        stop()
        boot("SCENARIO deployment COMPLETE", "deployment")
        stop()
        assert progress()["Operator"]["Deployment"] == 2
        assert progress()["Operator"]["StorageKey"] == "PredateRaven01_d2"
        boot("SELFTEST PASS")
        stop()
        reset_clone()
        native = DEV / "mission/solo.chernarusplus/storage_1/expansion/ai/PredateRaven01/group.bin"
        assert native.is_file(), native
        native.unlink()
        unchanged = (DEV / "profile/PredateProgress.json").read_bytes()
        boot("RECOVERY REQUIRED: Cannot restore")
        stop()
        assert (DEV / "profile/PredateProgress.json").read_bytes() == unchanged
        reset_clone()
        data = progress(); data["SchemaVersion"] = 999
        path = DEV / "profile/PredateProgress.json"
        path.write_text(json.dumps(data))
        unchanged = path.read_bytes()
        boot("RECOVERY REQUIRED: Unsupported config/save schema")
        stop()
        assert path.read_bytes() == unchanged
        reset_clone()
        path.write_text('{"broken":')
        boot("RECOVERY REQUIRED: Unreadable progress")
        stop()
        assert path.read_text() == '{"broken":'
        # Leave an ordinary usable development clone after error handling checks.
        reset_clone()
        assert release.fingerprints(ROOT / "world") == live_before, "Main world changed during development tests"
        print("PASS: main world byte-for-byte unchanged", flush=True)
    finally:
        world.select_profile("development")
        if world.running(): stop()
    report = {"schema": 1, "checks": RESULTS, "main_world_unchanged": True}
    release.write_changed(ROOT / "build/engine-foundation-results.json", json.dumps(report, indent=2) + "\n")


if __name__ == "__main__": main()
