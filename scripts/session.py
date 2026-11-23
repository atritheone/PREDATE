"""PREDATE session supervisor and local request interface. Standard library only."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import time
import uuid

import platform_win
import mod_presets
import session_store as store
import world
import world_pause
from client_window import ClientLaunchFocus

from app_runtime import ROOT, command as helper_command, saves_directory
RUNTIME = ROOT / "session-runtime"
STATUS = RUNTIME / "status.json"
MANAGER = RUNTIME / "manager.json"
OPERATION_SETUP_VERSION = 8
_status_cache = None


def read_native(path):
    try: return store.read_json(path, {})
    except (OSError, ValueError): return {}


def manager_alive():
    identity = read_native(MANAGER)
    if not identity: return False
    return world.process_identity(identity.get("pid", 0)) == identity


def ensure_manager():
    RUNTIME.mkdir(exist_ok=True)
    if manager_alive(): return
    log = (RUNTIME / "supervisor.log").open("ab")
    world.launch_hidden(helper_command("session", "serve", windowless=True), ROOT, log)
    log.close()
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if manager_alive(): return
        time.sleep(0.1)
    raise RuntimeError("The PREDATE session supervisor did not start. Inspect session-runtime/supervisor.log")


def request(action, **values):
    if action in {"new", "continue"} and not platform_win.find_processes("steam.exe"):
        raise RuntimeError("Steam is not running. Open Steam, then try again.")
    ensure_manager()
    required = None
    if action == "new" and "setup" in values:
        required = ("operation_setup_version", OPERATION_SETUP_VERSION)
    elif action == "delete":
        required = ("can_delete_saves", True)
    if required:
        capability, expected = required
        deadline = time.monotonic() + 2
        while state().get(capability) != expected:
            if time.monotonic() >= deadline:
                raise RuntimeError("An older PREDATE session is still running. Restart the session supervisor and try again.")
            time.sleep(0.1)
    identifier = uuid.uuid4().hex
    store.atomic_json(RUNTIME / "requests" / (identifier + ".json"),
                      {"id": identifier, "action": action, **values})
    return identifier


def wait_result(identifier, timeout=300):
    deadline = time.monotonic() + timeout
    path = RUNTIME / "results" / (identifier + ".json")
    while time.monotonic() < deadline:
        result = read_native(path)
        if result:
            if not result.get("ok"): raise RuntimeError(result.get("error", "Session request failed"))
            return result
        time.sleep(0.1)
    raise TimeoutError("The session is still busy; see the launcher for its current state")


def state():
    global _status_cache
    owner = read_native(MANAGER)
    # A transient file-sharing/read failure is not evidence that supervision
    # stopped. Revalidate the cached process identity rather than inventing idle.
    if not owner and _status_cache:
        owner = _status_cache[0]
    if not owner or world.process_identity(owner.get("pid", 0)) != owner:
        _status_cache = None
        return {"phase": "idle", "message": "Ready to continue", "profile": store.preferences()["selected_world"]}
    result = read_native(STATUS)
    updated = result.get("updated") if isinstance(result, dict) else None
    started = (owner.get("created", 0) - 116444736000000000) / 10000000
    current_generation = updated is None or (isinstance(updated, (int, float)) and updated >= started)
    if isinstance(result, dict) and current_generation and isinstance(result.get("phase"), str) and result["phase"] and isinstance(result.get("message"), str):
        _status_cache = (dict(owner), dict(result))
        return dict(result)
    if _status_cache and _status_cache[0] == owner:
        return dict(_status_cache[1])
    return {"phase": "connecting", "message": "Reading session status...", "profile": store.preferences()["selected_world"]}


class Supervisor:
    def __init__(self):
        self.lock = threading.RLock()
        self.job = None
        self.cancel = threading.Event()
        self.client = None
        self.profile = None
        self.session_id = None
        self.seen_connection = False
        self.disconnected_at = None
        self.command_sequence = 0
        self.connection_deadline = None
        self.last_backup = None
        self.last_activity = time.monotonic()
        self.data = {"phase": "idle", "message": "Ready to continue", "profile": None, "error": "",
                     "operation_setup_version": OPERATION_SETUP_VERSION, "can_delete_saves": True}
        self.quit_waiters = []
        self.pause_preparing = None
        self.pause_resume_sequence = None
        self.pause_resuming = False
        self.pause_reconnect = False
        self.pause_retry_at = 0
        self.failed_server_exit_sent = False
        self.client_launch_focus = None

    def publish(self, phase=None, message=None, **values):
        with self.lock:
            if phase is not None: self.data["phase"] = phase
            if message is not None: self.data["message"] = message
            self.data.update(values)
            self.data["updated"] = time.time()
            store.atomic_json(STATUS, self.data)

    def result(self, command, ok=True, **values):
        store.atomic_json(RUNTIME / "results" / (command["id"] + ".json"), {"ok": ok, **values})

    def configure(self, profile, validate=True):
        world.select_profile(profile)
        if not validate: return
        verified = store.verify_operation_mods(profile)
        world.MODS.extend(Path(mod["path"]) for mod in verified["mods"] if Path(mod["path"]) not in world.MODS)
        mod_presets.install_keys(verified, world.SERVER / 'keys')

    def client_alive(self):
        return bool(self.client and world.process_identity(self.client["pid"]) == self.client)

    def client_status(self):
        if not self.profile: return {}
        info = read_native(store.world_path(self.profile) / "client/PredateClientStatus.json")
        return info if info.get("SessionId") == self.session_id else {}

    def command_client(self, action):
        acknowledged = self.client_status().get("Acknowledged", 0)
        self.command_sequence = max(self.command_sequence, acknowledged) + 1
        store.atomic_json(store.world_path(self.profile) / "client/PredateClientCommand.json", {
            "Schema": 1, "SessionId": self.session_id, "Sequence": self.command_sequence,
            "Action": action, "Port": world.PORT, "QueryPort": world.QUERY_PORT,
            "Password": world.settings()["join_password"] if action in ("connect", "server-ready") else ""})

    def start_operation(self, profile, launcher_pid=None, diagnostics=None):
        store.existing_world_path(profile)
        if self.profile and self.profile != profile:
            self.save_quit()
        if self.profile == profile and world.running() and self.client_alive():
            if self.data['phase'] == 'paused' or world_pause.suspended(self.session_id):
                self.command_client('request-resume')
            else: self.command_client("connect")
            self.publish("connecting", "Reconnecting to your saved operation...", error="")
            self.disconnected_at = None
            return
        if platform_win.find_processes("DayZ_x64.exe"):
            raise RuntimeError("DayZ is already open outside this session. Close it before continuing here.")
        self.configure(profile)
        if profile == "main" and world.stable_fallback():
            raise RuntimeError("The new launcher is still being validated; the original operation remains protected.")
        if world.running():
            raise RuntimeError("This world's server was started separately. Save and stop it before using the launcher.")
        previous = read_native(store.world_path(profile) / "last-session.json")
        if previous and not previous.get("clean", True):
            backup = previous.get("backup")
            if not backup: raise RuntimeError("The previous session was unclean and no recovery snapshot is recorded")
            recovered = store.restore_snapshot(backup, store.metadata(profile)["name"] + " (recovered)")
            profile = recovered["id"]
            self.configure(profile)
        self.profile = profile
        self.session_id = uuid.uuid4().hex
        self.client = None
        self.client_launch_focus = ClientLaunchFocus(launcher_pid) if launcher_pid else None
        self.command_sequence = 0
        self.seen_connection = False
        self.disconnected_at = None
        self.pause_preparing = None
        self.pause_resume_sequence = None
        self.pause_resuming = False
        self.pause_reconnect = False
        self.failed_server_exit_sent = False
        self.publish("starting", "Preparing your operation...", profile=profile, session_id=self.session_id, error="")
        world.setup()
        # A pre-launch recovery point is always taken from stopped stores.
        self.last_backup = store.snapshot(profile, reason="before-continue")
        store.atomic_json(store.world_path(profile) / "last-session.json", {
            "schema": 1, "session_id": self.session_id, "clean": False,
            "backup": self.last_backup.name, "started": time.time()})
        prefs = store.save_preferences({"selected_world": profile})
        launched_at = None

        def open_client():
            nonlocal launched_at
            if self.cancel.is_set():
                return
            launched_at = time.time()
            startup_args = ["-predateStartupFocus"] if self.client_launch_focus else None
            world.launch_client(self.session_id, prefs["offline"], extra_args=startup_args, wait_for_server=True)

        def loading_progress(_):
            if launched_at is not None:
                self.observe_starting_client(launched_at)
            if self.data["phase"] != "loading":
                self.publish("loading", "Opening DayZ and loading the world...")

        try:
            world.start(self.session_id, prefs["offline"], progress=loading_progress,
                        cancelled=self.cancel.is_set, on_started=open_client, diagnostics=diagnostics)
            if self.cancel.is_set():
                return
            if launched_at is None:
                raise RuntimeError("This world's server was started separately.")
            self.command_client("server-ready")
            self.publish("connecting", "Connecting to the restored world...")
            self.connection_deadline = time.monotonic() + 180
            deadline = time.monotonic() + 150
            while time.monotonic() < deadline:
                if self.observe_starting_client(launched_at):
                    return
                if not world.running(): raise RuntimeError("The server exited while the game was opening")
                if self.cancel.is_set():
                    return
                time.sleep(0.25)
            raise RuntimeError("DayZ did not open. Check Steam before retrying.")
        except Exception:
            if launched_at is not None:
                self.command_client("server-closed")
            if world.running() and not self.cancel.is_set():
                self.stop_server("startup-failed")
            raise
        finally:
            if self.session_id and launched_at is not None and self.cancel.is_set() and not self.client_alive():
                self.command_client("server-closed")

    def observe_starting_client(self, launched_at):
        failure = read_native(store.world_path(self.profile) / "client/PredateFailure.json")
        failed_process = failure.get("process", {})
        created = failed_process.get("created", 0) / 10_000_000 - 11_644_473_600
        if created >= launched_at - 0.1 and failed_process.get("executable", "").casefold() == str(world.CLIENT / "DayZ_x64.exe").casefold():
            raise RuntimeError("DayZ failed to start: " + failure.get("message", "Unknown client error"))
        if self.client and not self.client_alive():
            raise RuntimeError("DayZ closed before the world was ready.")
        if not self.client:
            for pid in platform_win.find_processes("DayZ_x64.exe"):
                identity = world.process_identity(pid)
                if not identity: continue
                self.client = identity
                store.atomic_json(RUNTIME / "active-session.json", {
                    "profile": self.profile, "session_id": self.session_id, "client": identity,
                    "server": read_native(world.STATE)})
                break
        self.handoff_client_focus(connected=False)
        return bool(self.client)

    def handoff_client_focus(self, connected):
        if not self.client_launch_focus or not self.client_alive(): return
        try:
            if self.client_launch_focus.poll(self.client["pid"], connected):
                self.client_launch_focus = None
        except OSError as error:
            self.client_launch_focus = None
            print(f"DayZ startup activation failed: {error}", flush=True)

    def stop_server(self, reason):
        if not self.profile: return
        self.configure(self.profile, validate=False)
        if self.session_id: world_pause.resume(self.session_id)
        self.publish("stopping", "Saving the world and stopping its server...")
        if world.running():
            world.stop(make_backup=False)
        identity = read_native(world.STATE)
        failed = identity.get('session_id') == self.session_id and world.process_failure(identity)
        native = world.native_status()
        acknowledged_stop = native.get("SessionId") == self.session_id and native.get("Phase") == "stopping"
        if self.session_id and (failed or not acknowledged_stop):
            self.client = None
            self.session_id = None
            (RUNTIME / "active-session.json").unlink(missing_ok=True)
            self.publish("error", "The server had already stopped unexpectedly. Continue will recover its last validated snapshot.",
                         error="Unclean server exit; last validated snapshot retained")
            return
        self.publish("saving", "Validating the saved character and world...")
        self.last_backup = store.snapshot(self.profile, self.session_id or "", reason)
        store.atomic_json(store.world_path(self.profile) / "last-session.json", {
            "schema": 1, "session_id": self.session_id, "clean": True,
            "backup": self.last_backup.name, "finished": time.time()})
        (RUNTIME / "active-session.json").unlink(missing_ok=True)
        self.publish("suspended", "Saved and suspended. Continue when you are ready.",
                     backup=self.last_backup.name, grace_remaining=None, error="")
        self.client = None
        self.session_id = None

    def save_quit(self):
        if not self.profile:
            self.publish("idle", "No operation is running")
            return
        if not self.session_id:
            # In particular, do not validate stores retained after a failed
            # session merely because Save and Quit was requested again.
            return
        self.configure(self.profile, validate=False)
        # Retain the native actor hold while waking the process to service saves.
        if self.session_id: world_pause.resume(self.session_id)
        if self.client_alive():
            self.publish("saving", "Saving your character and closing DayZ...")
            server_closed = not world.running()
            self.command_client("server-closed" if server_closed else "quit")
            deadline = time.monotonic() + (10 if server_closed else 60)
            while time.monotonic() < deadline and self.client_alive():
                if not server_closed and not world.running():
                    server_closed = True
                    self.command_client('server-closed')
                    deadline = time.monotonic()+10
                info = self.client_status()
                if not server_closed and info.get("Acknowledged", 0) >= self.command_sequence and info.get("Error"):
                    raise RuntimeError(info["Error"])
                time.sleep(0.25)
            if self.client_alive():
                raise RuntimeError("DayZ has not acknowledged Save and Quit. It was retained; retry when it responds.")
        self.stop_server("save-and-quit")

    def dispatch(self, command):
        action = command.get("action")
        if action not in {"continue", "quit", "new", "restore", "backup", "delete"}:
            self.result(command, False, error="Unknown session action")
            return
        if self.job and self.job.is_alive():
            if action == "quit":
                self.cancel.set()
                self.quit_waiters.append(command)
                self.publish(message="Finishing the current save/start safely, then stopping...")
            else: self.result(command, False, error="PREDATE is already starting or saving an operation")
            return
        self.cancel.clear()
        self.job = threading.Thread(target=self.run_job, args=(command,), daemon=False)
        self.job.start()

    def delete_operation(self, profile):
        if self.session_id or self.client_alive() or (RUNTIME / "active-session.json").exists():
            raise RuntimeError("Save and quit the current hunt before deleting saves.")
        store.existing_world_path(profile)
        self.configure(profile, validate=False)
        if world.running():
            raise RuntimeError("Save and quit the current hunt before deleting saves.")
        store.delete_operation(profile)
        if self.profile == profile:
            self.profile = None
            self.last_backup = None
        if self.data.get("profile") == profile:
            self.publish("idle", "Ready to continue", profile=None, session_id=None, backup=None, error="")

    def run_job(self, command):
        action = command["action"]
        try:
            if action == "continue":
                self.start_operation(command.get("profile") or store.preferences()["selected_world"],
                                     command.get("launcher_pid"), command.get("diagnostics"))
            elif action == "quit": self.save_quit()
            elif action == "new":
                info = store.create_operation(command.get("name", ""), player_loadout=command.get('loadout', 'm4a1'),
                                              opponent_count=command.get('opponents', store.DEFAULT_OPPONENT_COUNT),
                                              footprints=command.get('footprints', True), spawn=command.get('spawn'),
                                              setup=command.get('setup'), require_modern_gear=True)
                self.start_operation(info["id"], command.get("launcher_pid"), command.get("diagnostics"))
            elif action == "restore":
                store.validate_snapshot(ROOT / "backups" / command["backup"])
                if self.profile and (self.client_alive() or world.running()): self.save_quit()
                info = store.restore_snapshot(command["backup"], command["name"])
                store.save_preferences({"selected_world": info["id"]})
                self.publish("suspended", "Backup restored as a separate operation. Select Continue to play.",
                             profile=info["id"], error="")
            elif action == "backup":
                profile = command.get("profile") or store.preferences()["selected_world"]
                store.existing_world_path(profile)
                self.configure(profile)
                if world.running():
                    raise RuntimeError("Use Save and Quit to create a consistent snapshot of an active world")
                path = store.snapshot(profile)
                self.publish(message="Validated snapshot created: " + path.name)
            elif action == "delete":
                self.delete_operation(command["profile"])
            if command.get("failure"):
                self.publish("error", command["failure"], error=command["failure"])
            self.result(command)
        except Exception as error:
            if action == "delete":
                self.result(command, False, error=str(error))
            elif self.cancel.is_set():
                self.result(command, cancelled=True)
            else:
                if self.profile and not self.client_alive() and world.running():
                    try: world.stop(make_backup=False)
                    except Exception: pass
                self.publish("error", str(error), error=str(error))
                self.result(command, False, error=str(error))
        finally:
            if self.cancel.is_set():
                try:
                    self.save_quit()
                    for item in self.quit_waiters: self.result(item)
                except Exception as error:
                    for item in self.quit_waiters: self.result(item, False, error=str(error))
                self.quit_waiters.clear()
            self.cancel.clear()

    def monitor_pause(self, native, client):
        """Hold server gameplay while native UI and local transport stay responsive."""
        if not self.seen_connection: return False
        # Native JSON writes can briefly be incomplete. Missing data is never
        # evidence that the player clicked Resume.
        if not client:
            return bool(native.get('Paused') or self.pause_preparing or self.pause_resuming or world_pause.suspended(self.session_id))
        if client.get('Paused'):
            self.pause_resuming = False
            self.pause_resume_sequence = None
            if not world_pause.suspended(self.session_id):
                if not native.get('Paused') or not native.get('PauseReady'):
                    if self.pause_preparing is None:
                        self.pause_preparing = (world.control('pause'), time.monotonic() + 5)
                        self.publish('pausing', 'Pausing the operation...')
                    elif time.monotonic() > self.pause_preparing[1]:
                        raise RuntimeError('The server did not acknowledge pause; the operation was retained')
                    return True
                world_pause.pause(self.session_id)
                self.command_client('paused')
            self.publish('paused', 'Operation paused. Resume returns to the same world.', grace_remaining=None)
            return True

        held = native.get('Paused') or self.pause_preparing is not None
        if world_pause.suspended(self.session_id):
            world_pause.resume(self.session_id)
            self.pause_resuming = True
            self.pause_reconnect = client.get('PauseDisconnected') or client.get('Phase') != 'connected'
            self.pause_retry_at = time.monotonic() + .5
            self.connection_deadline = time.monotonic() + 180
        elif held:
            if not self.pause_resuming: self.connection_deadline = time.monotonic() + 180
            self.pause_resuming = True
        if not self.pause_resuming: return False
        if client.get('Error'): raise RuntimeError(client['Error'])
        connected = client.get('Phase') == 'connected' and not client.get('PauseDisconnected')
        if connected:
            if self.pause_resume_sequence is None:
                self.pause_resume_sequence = world.control('resume')
            elif native.get('Acknowledged', 0) >= self.pause_resume_sequence and not native.get('Paused'):
                self.command_client('resume')
                self.pause_resuming = False
                self.pause_reconnect = False
                self.pause_preparing = None
                self.pause_resume_sequence = None
                self.disconnected_at = None
                return False
        elif time.monotonic() >= self.pause_retry_at and native.get('Players', 0) == 0:
            self.command_client('connect')
            self.pause_retry_at = time.monotonic() + 5
        if self.connection_deadline and time.monotonic() > self.connection_deadline:
            raise RuntimeError('The paused operation could not reconnect. Save and Quit retains the world.')
        self.publish('connecting', 'Resuming your paused operation...', grace_remaining=None)
        return True

    def monitor(self):
        if not self.profile or not self.session_id: return
        if self.job and self.job.is_alive(): return
        self.configure(self.profile, validate=False)
        if not world.running():
            if not self.client_alive():
                self.dispatch({'id':uuid.uuid4().hex, 'action':'quit'})
                return
            if not self.failed_server_exit_sent:
                self.command_client('server-closed')
                self.failed_server_exit_sent = True
            if self.data["phase"] != "error":
                self.publish("error", "The server stopped unexpectedly. The previous validated backup is retained.",
                             error="Unexpected server exit")
            return
        # Never keep simulating a closed game through a reconnect grace period.
        if self.client and not self.client_alive():
            command = {"id": uuid.uuid4().hex, "action": "quit"}
            failure = world.process_failure(self.client, store.world_path(self.profile) / "client")
            if failure: command["failure"] = "DayZ stopped after an error. Your world was saved: " + failure["message"]
            self.dispatch(command)
            return
        native = world.native_status()
        if native.get("SessionId") != self.session_id: return
        client = self.client_status()
        self.handoff_client_focus(client.get("Phase") == "connected" and native.get("Players", 0) > 0)
        client_error = client.get("Error")
        if client_error and not self.seen_connection:
            self.dispatch({"id": uuid.uuid4().hex, "action": "quit", "failure": client_error})
            return
        if native.get("Error"):
            self.publish("error", native["Error"], error=native["Error"])
            return
        try:
            if self.monitor_pause(native, client): return
        except Exception as error:
            self.dispatch({'id': uuid.uuid4().hex, 'action': 'quit', 'failure': str(error)})
            return
        if native.get("Players", 0) > 0:
            self.seen_connection = True
            self.disconnected_at = None
            self.publish("playing", "Operation running. Escape pauses the world.", grace_remaining=None)
        elif self.seen_connection:
            if self.disconnected_at is None: self.disconnected_at = time.monotonic()
            remaining = max(0, store.preferences()["reconnect_grace"] - (time.monotonic() - self.disconnected_at))
            self.publish("reconnecting", "Disconnected. Continue can reconnect while DayZ remains open.",
                         grace_remaining=round(remaining))
            if remaining <= 0:
                self.dispatch({"id": uuid.uuid4().hex, "action": "quit"})
        elif self.connection_deadline is not None and time.monotonic() >= self.connection_deadline:
            self.dispatch({"id": uuid.uuid4().hex, "action": "quit",
                           "failure": "DayZ did not connect to the ready world. It was saved and stopped; select Continue to retry."})

    def recover(self):
        active = read_native(RUNTIME / "active-session.json")
        if not active: return
        self.profile = active["profile"]
        self.session_id = active["session_id"]
        self.client = active["client"]
        self.configure(self.profile, validate=False)
        # An interrupted owner may have suspended the server. Wake it with the
        # native actor hold intact; monitor_pause will restore the requested state.
        world_pause.resume(self.session_id)
        self.seen_connection = True
        self.publish("reconnecting", "Recovered supervision of the existing session", profile=self.profile,
                     session_id=self.session_id)

    def serve(self):
        saves_directory(ROOT)
        handle = platform_win.singleton(ROOT)
        if handle is None: return
        store.atomic_json(MANAGER, world.process_identity(os.getpid()))
        self.publish("idle", "Ready to continue")
        self.recover()
        try:
            while True:
                for path in sorted((RUNTIME / "requests").glob("*.json"), key=lambda p: p.stat().st_mtime_ns):
                    command = read_native(path)
                    if not command: continue
                    path.unlink()
                    self.dispatch(command)
                self.monitor()
                time.sleep(0.25)
        finally:
            MANAGER.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["serve", "continue", "quit", "new", "restore", "backup", "delete", "status"])
    parser.add_argument("--profile")
    parser.add_argument("--name")
    parser.add_argument("--backup")
    parser.add_argument("--server-diagnostics", dest="diagnostics", action="store_true", default=None)
    args = parser.parse_args()
    if args.action == "serve": Supervisor().serve(); return
    if args.action == "status": print(json.dumps(state(), indent=2)); return
    identifier = request(args.action, **{key: value for key, value in vars(args).items() if key != "action" and value is not None})
    print(json.dumps(wait_result(identifier), indent=2))


if __name__ == "__main__":
    try: main()
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
