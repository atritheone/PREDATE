"""Local readiness and entitlement from DayZ's signed-in Steam client."""
import ctypes
from functools import lru_cache
import json
from pathlib import Path
import sys
import time
import uuid
import winreg

from app_runtime import ROOT, command
import maps
import platform_win
import release
import world


DIRECTORY = ROOT / "session-runtime" / "dlc"
RESULT = DIRECTORY / "entitlements.json"
RETRY_AFTER = 60
REFRESH_AFTER = 900


def read(path):
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError):
        return {}


def steam_session():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam\ActiveProcess") as key:
            account = winreg.QueryValueEx(key, "ActiveUser")[0]
            pid = winreg.QueryValueEx(key, "pid")[0]
        identity = world.process_identity(pid)
        if account > 0 and identity and Path(identity["executable"]).name.lower() == "steam.exe":
            return [account, pid, identity["created"], (world.CLIENT / "DayZ_x64.exe").stat().st_mtime_ns]
    except OSError:
        pass
    return None


@lru_cache(maxsize=1)
def packages(lock_stamp):
    return read(release.LOCK).get("terrain_packages", {})


def files_ready(terrain):
    checks = packages(release.LOCK.stat().st_mtime_ns).get(terrain.id, {})
    for side, base in (("client", world.CLIENT), ("server", world.SERVER)):
        files = checks.get(side)
        if not files or any(not (base / terrain.id / name).is_file() for name in files):
            return False
    return (world.SERVER / "mpmissions" / terrain.native_mission / "db/events.xml").is_file()


def availability():
    session = steam_session()
    result = read(RESULT)
    same_session = session and result.get("steam") == session
    licenses = result.get("licenses", {}) if same_session and time.time() - result.get("checked", 0) < REFRESH_AFTER else {}
    return {item.id: (not item.dlc or (licenses.get(item.id) is True and files_ready(item)))
            for item in maps.TERRAINS.values()}


def require(map_id):
    selected = maps.terrain(map_id)
    if selected.dlc and not availability()[map_id]:
        if not files_ready(selected):
            raise ValueError(f"{selected.name} needs its {selected.dlc} map files. Finish the DayZ download in Steam and reopen PREDATE.")
        raise ValueError(f"{selected.name} requires {selected.dlc} on the current Steam account. Open New Hunt to refresh map availability.")


def start_probe():
    current = steam_session()
    previous = read(RESULT)
    licenses = previous.get("licenses", {})
    retry = REFRESH_AFTER if licenses.get("sakhal") and licenses.get("nasdara") else RETRY_AFTER
    if (not current or not any(item.dlc and files_ready(item) for item in maps.TERRAINS.values())
            or (previous.get("steam") == current and time.time() - previous.get("checked", 0) < retry)
            or any(platform_win.find_processes(name) for name in ("DayZ_x64.exe", "DayZ_BE.exe", "DayZServer_x64.exe"))):
        return
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    with (DIRECTORY / "probe.log").open("ab") as log:
        world.launch_hidden(command("dlc_access", windowless=True), ROOT, log)


def wait_for_probe():
    deadline = time.monotonic() + 76
    while time.monotonic() < deadline:
        handle = platform_win.singleton(DIRECTORY)
        if handle:
            ctypes.windll.kernel32.CloseHandle(ctypes.c_void_p(handle))
            return
        time.sleep(.2)
    raise RuntimeError("The DayZ DLC check is still running. Try again after Steam is ready.")


def main():
    from session_store import atomic_json

    lock = platform_win.singleton(DIRECTORY)
    if not lock:
        return
    try:
        current = steam_session()
        if not current or any(platform_win.find_processes(name) for name in ("DayZ_x64.exe", "DayZ_BE.exe", "DayZServer_x64.exe")):
            return
        previous = read(RESULT)
        licenses = previous.get("licenses", {})
        retry = REFRESH_AFTER if licenses.get("sakhal") and licenses.get("nasdara") else RETRY_AFTER
        if "--candidate" not in sys.argv and previous.get("steam") == current and time.time() - previous.get("checked", 0) < retry:
            return
        nonce = uuid.uuid4().hex
        profile = DIRECTORY / "profile"
        profile.mkdir(parents=True, exist_ok=True)
        launch = time.time()
        addon = ROOT / "mods" / ("@PredateCandidate" if "--candidate" in sys.argv else "@Predate")
        mods = [*world.MODS[:-1], addon]
        client = world.launch_hidden([str(world.CLIENT / "DayZ_x64.exe"), "-noSplash", "-noPause", "-window",
                                      "-x=320", "-y=240", f"-profiles={profile}", f"-predateDlcProbe={nonce}",
                                      "-mod=" + ";".join(map(str, mods))], world.CLIENT)
        identity = world.process_identity(client.pid)
        if identity: world.guard_process(identity, profile, profile / "PredateFailure.json")
        until = time.monotonic() + 65
        report = {}
        try:
            while time.monotonic() < until and client.poll() is None:
                time.sleep(.25)
            report = read(profile / "PredateDlcStatus.json")
            if report.get("Nonce") == nonce and steam_session() == current:
                atomic_json(RESULT, {"steam": current, "checked": launch,
                                     "licenses": {"sakhal": report.get("Sakhal") in (True, 1),
                                                  "nasdara": report.get("Nasdara") in (True, 1)}})
            elif steam_session() == current:
                atomic_json(RESULT, {"steam": current, "checked": launch, "licenses": {}})
        finally:
            if client.poll() is None:
                client.terminate()
                client.wait(timeout=8)
    finally:
        ctypes.windll.kernel32.CloseHandle(ctypes.c_void_p(lock))


if __name__ == "__main__":
    main()
