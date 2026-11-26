"""Local DayZ prototype controls. Python standard library only."""
from __future__ import annotations

import argparse
from contextlib import nullcontext
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
from xml.etree import ElementTree
import maps

from app_runtime import ROOT, command as helper_command, saves_directory, spawn, spawn_server
from platform_win import private_server_desktop
from release import DEPENDENCIES
WORLD = ROOT / "world"
PROFILE = WORLD / "profile"
STORAGE = WORLD / "storage"
TERRAIN = maps.terrain()
MISSION = WORLD / "mission" / TERRAIN.mission
SETTINGS = ROOT / "config/settings.local.json"
STATE = WORLD / "server-process.json"
SERVER = ROOT / "runtime"
CLIENT = Path(r"C:\Program Files (x86)\Steam\steamapps\common\DayZ")
CLIENT_BOOTSTRAP = "DayZ_BE.exe"
DIAGNOSTIC_EXE = "DayZDiag_x64.exe"
PORT, QUERY_PORT, RCON_PORT = 2402, 2405, 2406

TEST_CLIENT = False
PROFILE_NAME = "main"
SCENARIO = None
CONFIG = ROOT / "serverDZ.cfg"
MODS = [ROOT / "mods" / name for name in (*DEPENDENCIES, "@Predate")]


def diagnostic_runtime():
    return os.environ.get("PREDATE_DIAGNOSTIC_RUNTIME", "1") == "1"


def runtime_name():
    return "dayz-diagnostic" if diagnostic_runtime() else "dayz-retail"


def client_executable():
    return CLIENT / (DIAGNOSTIC_EXE if diagnostic_runtime() else "DayZ_x64.exe")


def server_executable():
    return CLIENT / DIAGNOSTIC_EXE if diagnostic_runtime() else SERVER / "DayZServer_x64.exe"


def dayz_client_open():
    import platform_win

    # The official diagnostic server and client share the same executable.
    owned_server = json.loads(STATE.read_text())["pid"] if running() else None
    return any(pid != owned_server for name in ("DayZ_x64.exe", CLIENT_BOOTSTRAP, DIAGNOSTIC_EXE)
               for pid in platform_win.find_processes(name))


def select_profile(name, map_id=None):
    """Select a complete world, never just a different log directory."""
    global PROFILE_NAME, WORLD, PROFILE, STORAGE, MISSION, SETTINGS, STATE, CONFIG, TERRAIN
    global PORT, QUERY_PORT, RCON_PORT
    global MODS
    if not re.fullmatch(r"[a-z][a-z0-9-]{0,31}", name):
        raise ValueError("Profile names use lowercase letters, digits and hyphens (1–32 characters).")
    selected = ROOT / "world" if name == "main" else saves_directory(ROOT) / name
    if map_id is None:
        metadata_path = selected / "operation.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8")) if metadata_path.is_file() else {}
        selected_terrain = maps.operation_terrain(metadata)
    else:
        selected_terrain = maps.terrain(map_id)
    if name == "main" and selected_terrain.id != maps.DEFAULT_MAP:
        raise ValueError("The original hunt uses Chernarus.")
    PROFILE_NAME, WORLD, TERRAIN = name, selected, selected_terrain
    PROFILE = WORLD / "profile"
    STORAGE = WORLD / "storage"
    MISSION = WORLD / "mission" / TERRAIN.mission
    SETTINGS = ROOT / "config/settings.local.json" if name == "main" else WORLD / "settings.local.json"
    CONFIG = ROOT / "serverDZ.cfg" if name == "main" else WORLD / "serverDZ.cfg"
    STATE = WORLD / "server-process.json"
    PORT, QUERY_PORT, RCON_PORT = (2402, 2405, 2406) if name == "main" else (2502, 2505, 2506)
    MODS = [ROOT / "mods" / item for item in (*DEPENDENCIES, "@Predate")]
    if name == "main" and stable_fallback():
        MODS[-1] = ROOT / "mods" / "@PredateBaseline"
    if os.environ.get("PREDATE_CANDIDATE_MOD") == "1":
        if name == "development" or name.startswith("validation-"):
            MODS[-1] = ROOT / "mods" / "@PredateCandidate"


def stable_fallback():
    selection = ROOT / "config/active-release.local.json"
    return selection.exists() and json.loads(selection.read_text()).get("main") == "baseline"


def launch_mods():
    import quad_nod_shadows

    mods = list(MODS)
    buca_addons = {
        "buca_essentials.pbo", "buca_weapons.pbo", "buca_weapons2.pbo",
        "buca_weapons3.pbo", "buca_weapons_arma.pbo",
    }
    if any(buca_addons <= {file.name.casefold() for file in root.glob("[Aa]ddons/*.pbo")} for root in mods):
        for root in MODS:
            companion = root / "buca-precision"
            if (companion / "addons/PredateBucaPrecision.pbo").is_file():
                mods.append(companion)
                break
    quad_nods = quad_nod_shadows.companion(ROOT, mods)
    if quad_nods and quad_nods not in mods:
        mods.append(quad_nods)
    return mods


def existing_profiles():
    return ["main"] + sorted(p.name for p in saves_directory(ROOT).glob("*")
                             if p.is_dir() and (p / "settings.local.json").exists())

def native_mission_inputs():
    manifest = ROOT / "source" / "mission" / TERRAIN.base_manifest
    if not manifest.is_file():
        raise RuntimeError(f"{TERRAIN.name} mission input lock is missing. Update the PREDATE checkout.")
    base = json.loads(manifest.read_text())
    native = SERVER / "mpmissions" / TERRAIN.native_mission
    for rel, expected in base.items():
        original = native / rel
        if not original.is_file() or hashlib.sha256(original.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"{TERRAIN.name} mission input is missing or changed: {rel}. Verify the DayZ Server installation.")
    return native, base


def install_mission():
    """Copy authored mission files only. Native and Expansion saves are never inputs."""
    if running():
        raise RuntimeError(f"Stop {PROFILE_NAME} before updating its mission.")
    if PROFILE_NAME == "main" and stable_fallback():
        print("Main retains the previous prototype mission while Milestone A is under validation.", flush=True)
        return
    source = ROOT / "source" / "mission" / "template"
    native, base = native_mission_inputs()
    for rel, expected in base.items():
        original = native / rel
        target = MISSION / rel
        if TERRAIN.id != maps.DEFAULT_MAP and rel == "db/events.xml":
            events = ElementTree.parse(original)
            for event in events.getroot().findall("event"):
                if event.attrib.get("name") == "Player":
                    continue
                for field in ("active", "nominal", "min", "max"):
                    element = event.find(field)
                    if element is not None: element.text = "0"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(ElementTree.tostring(events.getroot(), encoding="utf-8", xml_declaration=True))
            continue
        if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest() != expected:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(original, target)
    for path in source.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(source)
        if TERRAIN.id != maps.DEFAULT_MAP and rel.as_posix() in ("db/events.xml", "expansion/settings/AILocationSettings.json"):
            continue
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
    inputs = (client_executable(),) if diagnostic_runtime() else (server_executable(), client_executable(), CLIENT / CLIENT_BOOTSTRAP)
    for path in inputs:
        if not path.is_file():
            raise RuntimeError(f"Missing installation: {path}")
    if MISSION.exists() and any(STORAGE.rglob("players.db")):
        raise RuntimeError("Mission already exists without settings; inspect before continuing.")
    PROFILE.mkdir(parents=True, exist_ok=True)
    STORAGE.mkdir(parents=True, exist_ok=True)
    install_mission()
    private = {"join_password": secrets.token_hex(12)}
    SETTINGS.parent.mkdir(parents=True, exist_ok=True)
    SETTINGS.write_text(json.dumps(private, indent=2), encoding="utf-8")
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
serverTime = "2026/MISSION_DATE/12/00";
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
adminLogPlayerList = 0;
class Missions
{
    class DayZ
    {
        template = "MISSION_TEMPLATE";
    };
};
'''.replace("JOIN_PASSWORD", private["join_password"]).replace("QUERY_PORT", str(QUERY_PORT)).replace(
        "MISSION_TEMPLATE", TERRAIN.mission).replace("MISSION_DATE", TERRAIN.start_date)
    CONFIG.write_text(managed_server_config(config), encoding="ascii")
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
    print(f"Created a separate {TERRAIN.name} world. Steam installation files were retained.", flush=True)


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


def managed_server_config(config):
    use_diagnostic = diagnostic_runtime()
    # The diagnostic server rejects a stock PBO with its signature handshake.
    # Only the password-protected loopback launch uses this configuration.
    for name, value in (("BattlEye", int(not use_diagnostic)), ("verifySignatures", 0 if use_diagnostic else 2),
                        ("allowFilePatching", 0), ("disable3rdPerson", 1), ("thirdPersonView", 0),
                        ("disableVoN", 1), ("adminLogPlayerList", 0)):
        config, count = re.subn(rf"(?im)^[ \t]*{name}\s*=\s*[^;\r\n]+;", f"{name} = {value};", config)
        if not count:
            config = f"{name} = {value};\n" + config
    return config


def start(session_id=None, progress=None, cancelled=None, on_started=None,
          diagnostics=None):
    import release
    import workshop_lasers
    release.verify()
    if TERRAIN.dlc:
        release.verify_terrain(TERRAIN, CLIENT, SERVER)
    native_mission_inputs()
    mods = launch_mods()
    workshop_lasers.verify_launch(mods)
    setup()
    settings()
    if running():
        print("Local world is already running.", flush=True)
        return
    if PROFILE_NAME == "main" and stable_fallback():
        raise RuntimeError("The original hunt is protected. Open the launcher to play its imported copy.")
    session_id = session_id or uuid.uuid4().hex
    config = CONFIG.read_text(encoding="utf-8")
    expected_template = re.escape(TERRAIN.mission)
    if not MISSION.is_dir() or not re.search(rf'(?im)^\s*template\s*=\s*"{expected_template}"\s*;', config):
        raise RuntimeError(f"The server configuration does not select {TERRAIN.name}.")
    updated_config = managed_server_config(config)
    if updated_config != config:
        CONFIG.write_text(updated_config, encoding="utf-8")
    # Refuse occupied ports instead of starting against another instance.
    for port in (PORT, QUERY_PORT):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("127.0.0.1", port))
    if diagnostics is None:
        diagnostics = PROFILE_NAME == "development" or PROFILE_NAME.startswith("validation-")
    binary = server_executable()
    if not binary.is_file(): raise RuntimeError(f"Missing DayZ server executable: {binary}")
    args = [str(binary)]
    if diagnostic_runtime(): args += ["-server", "-extrace=0"]
    args += [f"-config={CONFIG}",
            "-ip=127.0.0.1", f"-port={PORT}", f"-profiles={PROFILE}",
            f"-mission={MISSION}", f"-storage={STORAGE}",
            "-freezeCheck", "-limitFPS=60"]
    if diagnostics:
        args += ["-doLogs", "-adminLog", "-predateServerDiagnostics"]
    args.append("-mod=" + ";".join(str(p) for p in mods))
    args.append("-predateSessionId=" + session_id)
    if PROFILE_NAME == "development":
        args.append("-predateDevelopment")
        if SCENARIO: args.append("-predateScenario=" + SCENARIO)
    log = PROFILE / ("console-" + timestamp() + ".log") if diagnostics else None
    log_hint = str(log or PROFILE)
    with private_server_desktop() as desktop:
        with (log.open("wb") if log else nullcontext()) as output:
            pid = spawn_server(args, binary.parent, desktop, output)
        identity = process_identity(pid)
        if identity is None:
            raise RuntimeError(f"Server exited during startup. Inspect {log_hint}")
        identity.update(session_id=session_id, battleye=not diagnostic_runtime(), rcon_enabled=False)
        from session_store import atomic_json
        atomic_json(STATE, identity)
        process_identity_only = {key: identity[key] for key in ("pid", "created", "executable")}
        guard_process(process_identity_only, PROFILE, PROFILE / "PredateFailure.json", STATE)
        print("Starting local world. Initial world generation can take a few minutes...", flush=True)
        if on_started: on_started()
        deadline = time.monotonic() + 180
        while time.monotonic() < deadline:
            failure = process_failure(identity)
            if failure:
                raise RuntimeError(f"DayZ {failure['kind']} error: {failure['message']} (failed process closed; log: {failure['log']})")
            if not running():
                raise RuntimeError(f"Server exited. Inspect {log_hint}")
            if cancelled and cancelled():
                stop()
                raise RuntimeError("Launch cancelled; the server was stopped")
            status = native_status()
            if status.get("SessionId") == session_id:
                if status.get("Error"): raise RuntimeError(status["Error"])
                if status.get("Ready") and status.get("Phase") == "ready":
                    actual_map = status.get("WorldName", maps.DEFAULT_MAP).lower()
                    if actual_map != TERRAIN.id:
                        raise RuntimeError(f"DayZ opened {actual_map} instead of {TERRAIN.name}.")
                    print(f"PREDATE mission and operator are ready ({PROFILE_NAME}).", flush=True)
                    return
            if progress: progress("Loading the world and restoring its operator...")
            time.sleep(0.5)
        raise RuntimeError("Server is still running but readiness timed out. Inspect world/profile logs.")


def backup():
    if running():
        raise RuntimeError("Backup refused while the server is running.")
    folder = ROOT / "backups"
    folder.mkdir(exist_ok=True)
    map_label = TERRAIN.name.lower()
    destination = folder / (PROFILE_NAME + "-" + map_label + "-" + timestamp() + ".zip")
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        archive.write(CONFIG, "serverDZ.cfg")
        for path in MISSION.rglob("*"):
            if path.is_file():
                archive.write(path, f"mission/{TERRAIN.mission}/" + path.relative_to(MISSION).as_posix())
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
    # Older launchers used BattlEye RCon. Managed sessions use native saves.
    legacy_rcon = identity.get("rcon_enabled", identity.get("battleye", True))
    stop_sequence = None
    if identity.get("session_id"):
        stop_sequence = control("stop")
    elif legacy_rcon:
        try:
            reply = rcon("#shutdown", timeout=5)
            if reply.strip(): print(reply, flush=True)
        except (OSError, TimeoutError):
            pass
    started = time.monotonic()
    deadline = started + 120
    fallback_sent = False
    while time.monotonic() < deadline and running():
        if legacy_rcon and stop_sequence is not None and not fallback_sent and time.monotonic() - started >= 15:
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


def launch_client(session_id=None, extra_args=None, wait_for_server=False, launcher_pid=None):
    import workshop_lasers
    mods = launch_mods()
    workshop_lasers.verify_launch(mods)
    # Avoid launching another client over an existing DayZ session.
    if dayz_client_open():
        raise RuntimeError("DayZ is already open. Close it before using scripts/Play PREDATE.cmd.")
    binary = client_executable()
    bootstrap = binary if diagnostic_runtime() else CLIENT / CLIENT_BOOTSTRAP
    if not bootstrap.is_file():
        raise RuntimeError(f"Missing DayZ client launcher: {bootstrap}")
    private = settings()
    args = [str(bootstrap)]
    if diagnostic_runtime(): args.append("-extrace=0")
    else: args.extend(["0", "1", "-exe", binary.name])
    args.extend(["-noSplash", "-noPause"])
    if not wait_for_server:
        args += [f"-connect=127.0.0.1:{PORT}:{QUERY_PORT}", f"-port={PORT}",
                 f"-password={private['join_password']}"]
    if session_id:
        client_profile = WORLD / "client"
        client_profile.mkdir(exist_ok=True)
        args += ["-predateSessionId=" + session_id, "-profiles=" + str(client_profile)]
        if wait_for_server: args.append("-predateWaitForServer")
    if PROFILE_NAME == "development": args.append("-predateDevelopment")
    if TEST_CLIENT: args.extend(["-predateUITest", "-noPause"])
    args.append("-mod=" + ";".join(str(p) for p in mods))
    if extra_args: args += extra_args
    client_logs = WORLD / "client" if session_id else Path(os.environ["LOCALAPPDATA"]) / "DayZ"
    for argument in args:
        if argument.startswith("-profiles="):
            client_logs = Path(argument.split("=", 1)[1])
    client_logs.mkdir(parents=True, exist_ok=True)
    import presentation
    import session_store as store
    presentation.write_client_options(client_logs, store.preferences(), store.display_name(PROFILE_NAME))
    if PROFILE_NAME.startswith('validation-') and os.environ.get('PREDATE_PRESENTATION_TEST') == '1' and (client_logs/'PredatePresentationValidation.txt').is_file():
        args.append('-predatePresentationValidation')
    # This is the interactive game; its window is intentionally visible.
    launched_at = time.time()
    child = spawn(args, cwd=CLIENT)
    if diagnostic_runtime():
        from client_window import watch_diagnostic_client
        watch_diagnostic_client(child, launcher_pid)
        identity = process_identity(child.pid)
        if identity: guard_process(identity, client_logs, client_logs / "PredateFailure.json")
    else:
        guard_arguments = helper_command("process_guard", "--discover-client",
                           "--since", str(launched_at), "--exe", str(binary),
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
        identity = json.loads(STATE.read_text())
        status = native_status()
        if identity.get("session_id") and status.get("SessionId") == identity["session_id"]:
            report["players"] = status.get("Players", 0)
        elif identity.get("rcon_enabled", identity.get("battleye", True)):
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
    parser.add_argument("--server-diagnostics", action="store_true", default=None,
                        help="Enable server performance, console and admin logs for start")
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
    if args.action == "start": start(diagnostics=args.server_diagnostics)
    else: globals()[args.action]()


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, TimeoutError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr, flush=True)
        sys.exit(1)
