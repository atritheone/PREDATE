"""Packaged session reconnect, grace expiry and ordinary-exit integration checks."""
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from engine_sessions import Bridge, fixture
import platform_win
import session
import session_store as store
import world


def native_command(action):
    state = session.state()
    status = session.read_native(world.WORLD / "client/PredateClientStatus.json")
    store.atomic_json(world.WORLD / "client/PredateClientCommand.json", {
        "Schema": 1, "SessionId": state["session_id"],
        "Sequence": status.get("Acknowledged", 0) + 1, "Action": action})


def main():
    if platform_win.find_processes("DayZ_x64.exe") or platform_win.find_processes("DayZServer_x64.exe"):
        raise RuntimeError("Close the existing game/server first")
    preferences = store.preferences()
    profile = fixture("crashlab-primitive-send-d1612")
    bridge = Bridge()
    report = {"profile": profile, "passed": False}
    try:
        bridge.call("preferences", {"reconnect_grace": 30})
        bridge.wait_action(bridge.action("continue", profile=profile))
        bridge.wait_phase({"playing"})
        client = platform_win.find_processes("DayZ_x64.exe")
        native_command("disconnect")
        bridge.wait_phase({"reconnecting"}, 60)
        assert world.running() and platform_win.find_processes("DayZ_x64.exe") == client
        frozen = world.native_status()["SimulationSeconds"]
        time.sleep(2)
        assert abs(world.native_status()["SimulationSeconds"] - frozen) < .02
        bridge.wait_action(bridge.action("continue", profile=profile))
        bridge.wait_phase({"playing"}, 90)
        assert platform_win.find_processes("DayZ_x64.exe") == client
        report["reconnect_same_client"] = True
        report["hunt_clock_held_during_disconnect"] = True
        print("PASS: reconnect uses the existing client", flush=True)
        bridge.call("preferences", {"reconnect_grace": 3})
        native_command("disconnect")
        bridge.wait_phase({"reconnecting"}, 60)
        bridge.wait_phase({"suspended"}, 90)
        assert not world.running() and not platform_win.find_processes("DayZ_x64.exe")
        report["grace_expiry_saves_and_closes"] = True
        print("PASS: grace expiry saves and stops", flush=True)
        bridge.call("preferences", {"reconnect_grace": 120})
        bridge.wait_action(bridge.action("continue", profile=profile))
        bridge.wait_phase({"playing"})
        # The agent calls the engine's normal RequestExit(0), without Save and Quit.
        # This exercises the supervisor observing an ordinary process closure.
        native_command("server-closed")
        started = time.monotonic()
        bridge.wait_phase({"suspended"}, 45)
        elapsed = time.monotonic() - started
        assert elapsed < 30, elapsed
        assert not world.running() and not platform_win.find_processes("DayZ_x64.exe")
        report["normal_client_exit_skips_grace"] = True
        report["normal_exit_to_saved_seconds"] = round(elapsed, 2)
        report["passed"] = True
    finally:
        try:
            if world.running() or platform_win.find_processes("DayZ_x64.exe"):
                bridge.wait_action(bridge.action("quit"), 90)
        finally:
            try: bridge.close()
            finally:
                store.save_preferences({k: v for k, v in preferences.items() if k != "schema"})
                store.atomic_json(store.ROOT / "build/engine-lifecycle-results.json", report)
                print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__": main()
