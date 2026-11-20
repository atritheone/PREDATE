"""Local DayZ prototype controls. Python standard library only."""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime
import json
import hashlib
import re
from pathlib import Path
import secrets
import shutil
import socket
import struct
import subprocess
import sys
import time
import zipfile
import zlib
import uuid
import os

from app_runtime import ROOT, command as helper_command, spawn
WORLD = ROOT / "world"
PROFILE = WORLD / "profile"
STORAGE = WORLD / "storage"
MISSION = WORLD / "mission" / "solo.chernarusplus"
SETTINGS = ROOT / "config/settings.local.json"
STATE = WORLD / "server-process.json"
SERVER = ROOT / "runtime"
CLIENT = Path(r"C:\Program Files (x86)\Steam\steamapps\common\DayZ")
PORT, QUERY_PORT, RCON_PORT = 2402, 2405, 2406

TEST_CLIENT = False
PROFILE_NAME = "main"
SCENARIO = None
CONFIG = ROOT / "serverDZ.cfg"
MODS = [ROOT / "mods" / name for name in ['@CF', '@DabsFramework', '@ExpansionCore', '@ExpansionAI', '@Predate']]


def select_profile(name):
    """Select a complete world, never just a different log directory."""
    global PROFILE_NAME, WORLD, PROFILE, STORAGE, MISSION, SETTINGS, STATE, CONFIG
    global PORT, QUERY_PORT, RCON_PORT
    global MODS
    if not re.fullmatch(r"[a-z][a-z0-9-]{0,31}", name):
        raise ValueError("Profile names use lowercase letters, digits and hyphens (1–32 characters).")
    PROFILE_NAME = name
    WORLD = ROOT / "world" if name == "main" else ROOT / "worlds" / name
    PROFILE = WORLD / "profile"
    STORAGE = WORLD / "storage"
    MISSION = WORLD / "mission" / "solo.chernarusplus"
    SETTINGS = ROOT / "config/settings.local.json" if name == "main" else WORLD / "settings.local.json"
    CONFIG = ROOT / "serverDZ.cfg" if name == "main" else WORLD / "serverDZ.cfg"
    STATE = WORLD / "server-process.json"
    PORT, QUERY_PORT, RCON_PORT = (2402, 2405, 2406) if name == "main" else (2502, 2505, 2506)
    MODS = [ROOT / "mods" / item for item in ("@CF", "@DabsFramework", "@ExpansionCore", "@ExpansionAI", "@Predate")]
    if name == "main" and stable_fallback():
        MODS[-1] = ROOT / "mods" / "@PredateBaseline"
    if os.environ.get("PREDATE_CANDIDATE_MOD") == "1":
        if name == "development" or name.startswith("validation-"):
            MODS[-1] = ROOT / "mods" / "@PredateCandidate"


def stable_fallback():
    selection = ROOT / "config/active-release.local.json"
    return selection.exists() and json.loads(selection.read_text()).get("main") == "baseline"


def existing_profiles():
    return ["main"] + sorted(p.name for p in (ROOT / "worlds").glob("*")
                             if p.is_dir() and (p / "settings.local.json").exists())


def install_mission():
    """Copy authored mission files only. Native and Expansion saves are never inputs."""
    if running():
        raise RuntimeError(f"Stop {PROFILE_NAME} before updating its mission.")
    if PROFILE_NAME == "main" and stable_fallback():
        print("Main retains the previous prototype mission while Milestone A is under validation.", flush=True)
        return
    source = ROOT / "source" / "mission" / "template"
    base = json.loads((source.parent / "base-files.json").read_text())
    for rel, expected in base.items():
        original = SERVER / "mpmissions" / "dayzOffline.chernarusplus" / rel
        if hashlib.sha256(original.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"Native mission version changed: {rel}. Review the release lock first.")
        target = MISSION / rel
        if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest() != expected:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(original, target)
    for path in source.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(source)
        if rel.as_posix() == "init.c":
            raise RuntimeError("Author mission startup in bootstrap.c, server/ and factory.c; template/init.c is ambiguous.")
        if any(part.startswith("storage_") for part in rel.parts):
            raise RuntimeError(f"Save data found in mission source: {rel}")
        target = MISSION / rel
        if not target.exists() or path.read_bytes() != target.read_bytes():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    # Compose the mission entry point from complete authored files, without
    # substituting code into an older sandbox. Server behavior extends MissionServer.
    parts = [source.parent / "bootstrap.c"]
    parts += sorted((source.parent / "server").glob("*.c"))
    parts += [source.parent / "factory.c"]
    combined = "\n\n".join(p.read_text(encoding="utf-8") for p in parts)
    target = MISSION / "init.c"
    if not target.exists() or target.read_text(encoding="utf-8") != combined:
        target.write_text(combined, encoding="utf-8")


def timestamp():
    return datetime.now().strftime("%Y%m%d-%H%M%S-%f")


def settings():
    if not SETTINGS.exists():
        raise RuntimeError("Run 'py -3 scripts/world.py setup' from the project root first.")
    return json.loads(SETTINGS.read_text(encoding="utf-8"))


def process_identity(pid):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD,
                                               wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    handle = kernel.OpenProcess(0x1000, False, pid)
    if not handle:
        return None
    try:
        code = wintypes.DWORD()
        if not kernel.GetExitCodeProcess(handle, ctypes.byref(code)) or code.value != 259:
            return None
        size = wintypes.DWORD(32768)
        name = ctypes.create_unicode_buffer(size.value)
        if not kernel.QueryFullProcessImageNameW(handle, 0, name, ctypes.byref(size)):
            raise ctypes.WinError(ctypes.get_last_error())
        times = [wintypes.FILETIME() for _ in range(4)]
        if not kernel.GetProcessTimes(handle, *(ctypes.byref(t) for t in times)):
            raise ctypes.WinError(ctypes.get_last_error())
        created = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
        return {"pid": pid, "executable": name.value, "created": created}
    finally:
        kernel.CloseHandle(handle)


def running():
    if not STATE.exists():
        return False
    stored = json.loads(STATE.read_text(encoding="utf-8"))
    actual = process_identity(stored["pid"])
    return actual == {name: stored[name] for name in ("pid", "executable", "created")}


def native_status():
    try:
        return json.loads((PROFILE / "PredateSessionStatus.json").read_text())
    except (OSError, ValueError):
        return {}


def control(action):
    from session_store import atomic_json
    state = json.loads(STATE.read_text())
    session_id = state.get("session_id")
    if not session_id: raise RuntimeError("This server predates managed sessions")
    status = native_status()
    sequence = max(status.get("Acknowledged", 0), int(time.time() * 1000) % 1_000_000_000) + 1
    atomic_json(PROFILE / "PredateControl.json", {"Schema": 1, "SessionId": session_id,
                                                "Sequence": sequence, "Action": action})
    return sequence


def setup():
    if SETTINGS.exists():
        print("Existing world retained. Setup already complete.", flush=True)
        return
    for path in [SERVER / "DayZServer_x64.exe", CLIENT / "DayZ_BE.exe"]:
        if not path.is_file():
            raise RuntimeError(f"Missing installation: {path}")
    if MISSION.exists() and any(STORAGE.rglob("players.db")):
        raise RuntimeError("Mission already exists without settings; inspect before continuing.")
    PROFILE.mkdir(parents=True, exist_ok=True)
    STORAGE.mkdir(parents=True, exist_ok=True)
    install_mission()
    be = WORLD / "battleye"
    be.mkdir(exist_ok=True)
    shutil.copy2(SERVER / "battleye" / "BEServer_x64.dll", be)
    private = {"join_password": secrets.token_hex(12), "rcon_password": secrets.token_hex(16)}
    SETTINGS.parent.mkdir(parents=True, exist_ok=True)
    SETTINGS.write_text(json.dumps(private, indent=2), encoding="utf-8")
    (be / "BEServer_x64.cfg").write_text(
        f"RConPassword {private['rcon_password']}\nRConPort {RCON_PORT}\n"
        "RConIP 127.0.0.1\nRestrictRCon 0\n", encoding="ascii")
    config = '''hostname = "PREDATE - First Hunt";
description = "Private local persistence prototype";
password = "JOIN_PASSWORD";
passwordAdmin = "";
maxPlayers = 1;
verifySignatures = 2;
forceSameBuild = 1;
BattlEye = 1;
disableVoN = 1;
disable3rdPerson = 1;
thirdPersonView = 0;
disableCrosshair = 1;
disablePersonalLight = 1;
lightingConfig = 0;
serverTime = "2026/9/20/12/00";
serverTimeAcceleration = 1;
serverNightTimeAcceleration = 1;
serverTimePersistent = 1;
guaranteedUpdates = 1;
loginQueueConcurrentPlayers = 1;
loginQueueMaxPlayers = 1;
instanceId = 1;
storageAutoFix = 0;
steamQueryPort = QUERY_PORT;
logAverageFps = 60;
logMemory = 60;
adminLogPlayerList = 1;
class Missions
{
    class DayZ
    {
        template = "solo.chernarusplus";
    };
};
'''.replace("JOIN_PASSWORD", private["join_password"]).replace("QUERY_PORT", str(QUERY_PORT))
    CONFIG.write_text(config, encoding="ascii")
    if PROFILE_NAME == "development":
        (PROFILE / "PredateDevelopment.txt").write_text("Isolated development world; tests may change actors.\n")
    # Private operations have no login or server-hopping countdown.
    globals_file = MISSION / "db" / "globals.xml"
    globals_text = globals_file.read_text(encoding="utf-8-sig")
    for name, value in [("TimeLogin", 0), ("TimeLogout", 1), ("TimePenalty", 0), ("TimeHopping", 0)]:
        import re
        globals_text = re.sub(r'(<var name="' + name + r'" type="0" value=")\d+("\s*/>)',
                              lambda m: m[1] + str(value) + m[2], globals_text)
    globals_file.write_text(globals_text, encoding="utf-8")
    print("Created a separate Chernarus world. Steam installation files were retained.", flush=True)


def packet(payload):
    body = b"\xff" + payload
    return b"BE" + struct.pack("<I", zlib.crc32(body)) + body


def receive(sock):
    data = sock.recv(65535)
    if len(data) < 8 or data[:2] != b"BE" or data[6] != 255:
        raise RuntimeError("Invalid RCon packet")
    if struct.unpack("<I", data[2:6])[0] != zlib.crc32(data[6:]):
        raise RuntimeError("RCon checksum mismatch")
    return data[7:]


def rcon(command, timeout=3):
    private = settings()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.connect(("127.0.0.1", RCON_PORT))
        sock.settimeout(timeout)
        sock.send(packet(b"\x00" + private["rcon_password"][:32].encode("ascii")))
        if receive(sock) != b"\x00\x01":
            raise RuntimeError("RCon authentication failed")
        sock.send(packet(b"\x01\x00" + command.encode("ascii")))
        until = time.monotonic() + timeout
        parts = {}
        while time.monotonic() < until:
            data = receive(sock)
            if data[0] == 2:
                sock.send(packet(data[:2]))
                continue
            if data[:2] != b"\x01\x00":
                continue
            response = data[2:]
            if len(response) >= 3 and response[0] == 0:
                count, index = response[1:3]
                parts[index] = response[3:]
                if len(parts) < count:
                    continue
                response = b"".join(parts[i] for i in range(count))
            return response.decode("utf-8", errors="replace")
        raise TimeoutError("RCon command response timed out")


def launch_hidden(args, cwd, stdout=None):
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    kernel = ctypes.WinDLL("kernel32")
    previous = kernel.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    try:
        return spawn(args, cwd=cwd, startupinfo=startup, stdin=subprocess.DEVNULL,
                     stdout=stdout if stdout else subprocess.DEVNULL, stderr=subprocess.STDOUT)
    finally:
        kernel.SetErrorMode(previous)


def guard_process(identity, logdir, failure_file, server_state=None):
    arguments = helper_command("process_guard", "--pid", str(identity["pid"]),
                 "--created", str(identity["created"]), "--exe", identity["executable"],
                 "--logdir", str(logdir), "--failure", str(failure_file), windowless=True)
    if server_state: arguments += ["--server-state", str(server_state)]
    with (Path(logdir) / ("guard-" + str(identity["pid"]) + ".log")).open("ab") as output:
        return launch_hidden(arguments, ROOT, output)


def process_failure(identity, logdir=None):
    path = (Path(logdir) if logdir else PROFILE) / "PredateFailure.json"
    try:
        failure = json.loads(path.read_text())
        if failure.get("process") == {key: identity[key] for key in ("pid", "created", "executable")}:
            return failure
    except (OSError, ValueError, KeyError): pass
    return None


def first_person_config(config):
    for name, value in (("disable3rdPerson", 1), ("thirdPersonView", 0)):
        config, count = re.subn(rf"(?im)^[ \t]*{name}\s*=\s*[^;\r\n]+;", f"{name} = {value};", config)
        if not count:
            config = f"{name} = {value};\n" + config
    return config


def start(session_id=None, offline=False, progress=None, cancelled=None):
    import release
    import workshop_lasers
    release.verify()
    workshop_lasers.verify_launch(MODS)
    setup()
    settings()
    if running():
        print("Local world is already running.", flush=True)
        return
    if not (PROFILE_NAME == "main" and stable_fallback()):
        session_id = session_id or uuid.uuid4().hex
    # Offline operation uses the ordinary local executable and omits BattlEye.
    config = CONFIG.read_text(encoding="utf-8")
    config = re.sub(r"BattlEye\s*=\s*\d+;", f"BattlEye = {0 if offline else 1};", config)
    CONFIG.write_text(first_person_config(config), encoding="utf-8")
    # Refuse occupied ports instead of starting against another instance.
    for port in [PORT, QUERY_PORT, RCON_PORT]:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("127.0.0.1", port))
    log = PROFILE / ("console-" + timestamp() + ".log")
    args = [str(SERVER / "DayZServer_x64.exe"), f"-config={CONFIG}",
            "-ip=127.0.0.1", f"-port={PORT}", f"-profiles={PROFILE}",
            f"-mission={MISSION}", f"-storage={STORAGE}", f"-BEpath={WORLD / 'battleye'}",
            "-doLogs", "-adminLog", "-freezeCheck", "-limitFPS=60"]
    args.append("-mod=" + ";".join(str(p) for p in MODS))
    if session_id: args.append("-predateSessionId=" + session_id)
    if PROFILE_NAME == "development":
        args.append("-predateDevelopment")
        if SCENARIO: args.append("-predateScenario=" + SCENARIO)
    with log.open("wb") as output:
        process = launch_hidden(args, SERVER, output)
    identity = process_identity(process.pid)
    if identity is None:
        raise RuntimeError(f"Server exited during startup. Inspect {log}")
    if session_id: identity["session_id"] = session_id
    from session_store import atomic_json
    atomic_json(STATE, identity)
    process_identity_only = {key: identity[key] for key in ("pid", "created", "executable")}
    guard_process(process_identity_only, PROFILE, PROFILE / "PredateFailure.json", STATE)
    print("Starting local world. Initial world generation can take a few minutes...", flush=True)
    deadline = time.monotonic() + 180
    while time.monotonic() < deadline:
        failure = process_failure(identity)
        if failure:
            raise RuntimeError(f"DayZ {failure['kind']} error: {failure['message']} (failed process closed; log: {failure['log']})")
        if not running():
            raise RuntimeError(f"Server exited. Inspect {log}")
        if cancelled and cancelled():
            stop()
            raise RuntimeError("Launch cancelled; the server was stopped")
        if session_id:
            status = native_status()
            if status.get("SessionId") == session_id:
                if status.get("Error"): raise RuntimeError(status["Error"])
                if status.get("Ready") and status.get("Phase") == "ready":
                    print(f"PREDATE mission and operator are ready ({PROFILE_NAME}).", flush=True)
                    return
            if progress: progress("Loading the world and restoring its operator...")
            recent = sorted(PROFILE.glob("script*.log"), key=lambda p: p.stat().st_mtime_ns)
            if recent:
                script = recent[-1]
                if script.stat().st_mtime >= log.stat().st_mtime:
                    tail = script.read_text(errors="replace")[-6000:]
                    if "can't compile" in tail.lower():
                        # No world can have initialized after a script compilation failure.
                        process.terminate()
                        process.wait(timeout=10)
                        raise RuntimeError(f"DayZ script compilation failed. Inspect {script}")
            time.sleep(0.5)
            continue
        try:
            rcon("players", timeout=1)
            print(f"Local world ({PROFILE_NAME}) is accepting connections at 127.0.0.1:{PORT}.", flush=True)
            return
        except (OSError, RuntimeError, TimeoutError):
            time.sleep(2)
    raise RuntimeError("Server is still running but readiness timed out. Inspect world/profile logs.")


def backup():
    if running():
        raise RuntimeError("Backup refused while the server is running.")
    folder = ROOT / "backups"
    folder.mkdir(exist_ok=True)
    destination = folder / (PROFILE_NAME + "-chernarus-" + timestamp() + ".zip")
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        archive.write(CONFIG, "serverDZ.cfg")
        for path in MISSION.rglob("*"):
            if path.is_file():
                archive.write(path, "mission/solo.chernarusplus/" + path.relative_to(MISSION).as_posix())
        expansion = PROFILE / "ExpansionMod"
        if expansion.exists():
            for path in expansion.rglob("*"):
                if path.is_file():
                    archive.write(path, "profile/ExpansionMod/" + path.relative_to(expansion).as_posix())
        for path in PROFILE.glob("Predate*.json"):
            archive.write(path, "profile/" + path.name)
        for path in STORAGE.rglob("*"):
            if path.is_file():
                archive.write(path, "storage/" + path.relative_to(STORAGE).as_posix())
    print(f"Saved backup: {destination.name}", flush=True)
    return destination


def stop(make_backup=True):
    if not running():
        print("Local world is already stopped.", flush=True)
        return
    print("Requesting a clean server shutdown and save...", flush=True)
    identity = json.loads(STATE.read_text())
    stop_sequence = None
    if identity.get("session_id"):
        stop_sequence = control("stop")
    else:
        try:
            reply = rcon("#shutdown", timeout=5)
            if reply.strip(): print(reply, flush=True)
        except (OSError, TimeoutError):
            pass
    started = time.monotonic()
    deadline = started + 120
    fallback_sent = False
    while time.monotonic() < deadline and running():
        if stop_sequence is not None and not fallback_sent and time.monotonic() - started >= 15:
            status = native_status()
            saved = status.get("SessionId") == identity["session_id"]
            saved = saved and status.get("Acknowledged", 0) >= stop_sequence and status.get("Phase") == "stopping"
            if saved:
                expected = {key: identity[key] for key in ("pid", "created", "executable")}
                if process_identity(identity["pid"]) != expected:
                    raise RuntimeError("The owned server changed during shutdown")
                # SaveAll finished before the native stopping acknowledgement.
                # Recover a blocked script exit callback through native RCon;
                # this still requests a clean exit and never kills the process.
                fallback_sent = True
                try: rcon("#shutdown", timeout=5)
                except (OSError, RuntimeError, TimeoutError): pass
        time.sleep(1)
    if running():
        raise RuntimeError("Clean shutdown did not finish. Server retained; no forced termination or live backup.")
    print("Server stopped.", flush=True)
    if make_backup: backup()


def launch_client(session_id=None, offline=False, extra_args=None):
    import workshop_lasers
    workshop_lasers.verify_launch(MODS)
    # Avoid launching another client over an existing DayZ session.
    processes = subprocess.run(["tasklist", "/FI", "IMAGENAME eq DayZ_x64.exe", "/FO", "CSV", "/NH"],
                               capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    if '"DayZ_x64.exe"' in processes.stdout:
        raise RuntimeError("DayZ is already open. Close it before using scripts/Play PREDATE.cmd.")
    private = settings()
    if offline:
        args = [str(CLIENT / "DayZ_x64.exe")]
    else:
        args = [str(CLIENT / "DayZ_BE.exe"), "0", "1", "-exe", "DayZ_x64.exe"]
    args += [f"-connect=127.0.0.1:{PORT}:{QUERY_PORT}", f"-port={PORT}",
             f"-password={private['join_password']}", "-noSplash", "-noPause"]
    if session_id:
        client_profile = WORLD / "client"
        client_profile.mkdir(exist_ok=True)
        args += ["-predateSessionId=" + session_id, "-profiles=" + str(client_profile)]
    if PROFILE_NAME == "development": args.append("-predateDevelopment")
    if TEST_CLIENT: args.extend(["-predateUITest", "-noPause"])
    args.append("-mod=" + ";".join(str(p) for p in MODS))
    if extra_args: args += extra_args
    client_logs = WORLD / "client" if session_id else Path(os.environ["LOCALAPPDATA"]) / "DayZ"
    for argument in args:
        if argument.startswith("-profiles="):
            client_logs = Path(argument.split("=", 1)[1])
    client_logs.mkdir(parents=True, exist_ok=True)
    import presentation
    import session_store as store
    presentation.write_client_options(client_logs, store.preferences())
    if PROFILE_NAME.startswith('validation-') and os.environ.get('PREDATE_PRESENTATION_TEST') == '1' and (client_logs/'PredatePresentationValidation.txt').is_file():
        args.append('-predatePresentationValidation')
    # This is the interactive game; its window is intentionally visible.
    launched_at = time.time()
    child = spawn(args, cwd=CLIENT)
    guard_arguments = helper_command("process_guard", "--discover-client",
                       "--since", str(launched_at), "--exe", str(CLIENT / "DayZ_x64.exe"),
                       "--logdir", str(client_logs), "--failure", str(client_logs / "PredateFailure.json"), windowless=True)
    with (client_logs / "guard-client.log").open("ab") as output:
        launch_hidden(guard_arguments, ROOT, output)
    print("Launching DayZ and connecting to your local world.", flush=True)
    return child


def play():
    if PROFILE_NAME == "main" and (ROOT / "PREDATE.exe").is_file():
        subprocess.Popen([str(ROOT / "PREDATE.exe"), "--continue"], cwd=ROOT)
        print("Opening the PREDATE desktop launcher.", flush=True)
        return
    start()
    state = json.loads(STATE.read_text())
    launch_client(state.get("session_id"))


def status():
    files = [p for p in STORAGE.rglob("*") if p.is_file()] if STORAGE.exists() else []
    report = {"running": running(), "address": f"127.0.0.1:{PORT}", "profile": PROFILE_NAME, "world": str(WORLD),
              "persistence_files": len(files), "persistence_bytes": sum(p.stat().st_size for p in files)}
    if report["running"]:
        try:
            report["players"] = rcon("players")
        except (OSError, RuntimeError, TimeoutError) as error:
            report["rcon"] = str(error)
    print(json.dumps(report, indent=2), flush=True)


def test():
    global TEST_CLIENT
    if PROFILE_NAME != "development":
        raise RuntimeError("Tests require --profile development; your normal hunt is never a test range.")
    TEST_CLIENT = True
    play()

def main():
    global SCENARIO
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["setup", "start", "stop", "play", "status", "backup", "test"])
    parser.add_argument("--profile", default="main", help="main, development, or a new named world")
    parser.add_argument("--scenario", choices=["registry", "defeat", "deployment"], help="development server checks")
    args = parser.parse_args()
    select_profile(args.profile)
    if args.scenario and args.profile != "development":
        parser.error("--scenario requires --profile development")
    SCENARIO = args.scenario
    if args.action == "stop" and args.profile == "main":
        import session
        if session.manager_alive() and session.state().get("phase") not in {"idle", "suspended"}:
            session.wait_result(session.request("quit"))
            return
    globals()[args.action]()


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, TimeoutError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr, flush=True)
        sys.exit(1)
