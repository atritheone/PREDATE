import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch, Mock
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import session
import session_store as store
import world


class SnapshotTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.patches = [patch.object(store, "ROOT", self.root), patch.object(world, "ROOT", self.root)]
        for item in self.patches: item.start()
        world.select_profile("main")
        (self.root / "config").mkdir()
        (self.root / "config/release-lock.json").write_text('{"schema":1,"game":"fixture"}')
        (world.PROFILE).mkdir(parents=True)
        (world.STORAGE / "storage_1").mkdir(parents=True)
        with sqlite3.connect(world.STORAGE / "storage_1/players.db") as db:
            db.execute("CREATE TABLE characters (id TEXT, ammo INTEGER)")
            db.execute("INSERT INTO characters VALUES ('player',17)")
        db.close()
        self.group = world.MISSION / "storage_1/expansion/ai/PredateRaven01/group.bin"
        self.group.parent.mkdir(parents=True)
        self.group.write_bytes(b"saved native AI with injuries and ammunition")
        (world.PROFILE / "PredateProgress.json").write_text('{"Selected":1,"PulseRemaining":38}')

    def tearDown(self):
        for item in reversed(self.patches): item.stop()
        world.select_profile("main")
        self.directory.cleanup()

    def test_snapshot_contains_verified_character_ai_and_hunt(self):
        path = store.snapshot("main")
        manifest = store.validate_snapshot(path)
        self.assertIn("storage/storage_1/players.db", manifest["files"])
        self.assertIn("mission/solo.chernarusplus/storage_1/expansion/ai/PredateRaven01/group.bin", manifest["files"])
        self.assertIn("profile/PredateProgress.json", manifest["files"])
        with zipfile.ZipFile(path) as archive:
            self.assertNotIn("settings.local.json", archive.namelist())
            self.assertNotIn("serverDZ.cfg", archive.namelist())

    def test_tampered_backup_is_rejected(self):
        path = store.snapshot("main")
        with zipfile.ZipFile(path) as archive:
            entries = {name: archive.read(name) for name in archive.namelist()}
        entries["profile/PredateProgress.json"] = b'{"Selected":0}'
        with zipfile.ZipFile(path, "w") as archive:
            for name, content in entries.items(): archive.writestr(name, content)
        with self.assertRaisesRegex(ValueError, "hash mismatch"): store.validate_snapshot(path)

    def test_path_traversal_is_rejected_before_restore(self):
        path = self.root / "bad.zip"
        with zipfile.ZipFile(path, "w") as archive: archive.writestr("../outside", "unsafe")
        with self.assertRaisesRegex(ValueError, "Unsafe backup path"): store.validate_snapshot(path)
        self.assertFalse((self.root.parent / "outside").exists())

    def test_restore_creates_separate_world_and_preserves_original(self):
        path = store.snapshot("main")
        original = self.group.read_bytes()
        def setup():
            world.PROFILE.mkdir(parents=True)
            world.STORAGE.mkdir(parents=True)
            world.MISSION.mkdir(parents=True)
        with patch.object(world, "setup", side_effect=setup):
            info = store.restore_snapshot(path.name, "Recovered")
        restored = store.world_path(info["id"])
        self.assertNotEqual(restored, self.root / "world")
        self.assertEqual(self.group.read_bytes(), original)
        self.assertEqual((restored / "profile/PredateProgress.json").read_text(), '{"Selected":1,"PulseRemaining":38}')
        with sqlite3.connect(restored / "storage/storage_1/players.db") as db:
            self.assertEqual(db.execute("SELECT ammo FROM characters").fetchone()[0], 17)
        db.close()

    def test_settings_reject_invalid_grace_and_mod_paths(self):
        with self.assertRaises(ValueError): store.save_preferences({"reconnect_grace": -1})
        with self.assertRaises(ValueError): store.save_preferences({"extra_mods": ["relative/mod"]})
        self.assertFalse((self.root / "config/launcher-settings.local.json").exists())

    def test_stale_new_operation_mod_preset_does_not_block_selecting_a_saved_world(self):
        store.atomic_json(self.root / 'config/launcher-settings.local.json',
                          store.DEFAULTS | {'extra_mods': [str(self.root / 'removed-mod')]})
        result = store.save_preferences({'selected_world': 'saved-operation'})
        self.assertEqual(result['selected_world'], 'saved-operation')
        with self.assertRaisesRegex(ValueError, 'addons'):
            store.create_operation('Needs missing mod')


class LifecycleTests(unittest.TestCase):
    def supervisor(self):
        value = session.Supervisor()
        value.profile = "test-world"
        value.session_id = "owned-session"
        value.client = {"pid": 123}
        value.configure = Mock()
        value.publish = Mock()
        value.dispatch = Mock()
        return value

    def test_closed_game_bypasses_reconnect_grace(self):
        value = self.supervisor()
        with patch.object(world, "running", return_value=True), patch.object(value, "client_alive", return_value=False):
            value.monitor()
        self.assertEqual(value.dispatch.call_args.args[0]["action"], "quit")

    def test_open_game_gets_reconnect_grace_then_stops(self):
        value = self.supervisor()
        value.seen_connection = True
        with patch.object(world, "running", return_value=True), patch.object(value, "client_alive", return_value=True), \
             patch.object(world, "native_status", return_value={"SessionId": "owned-session", "Players": 0}), \
             patch.object(store, "preferences", return_value={"reconnect_grace": 30}), \
             patch.object(session.time, "monotonic", return_value=100):
            value.monitor()
            value.dispatch.assert_not_called()
        with patch.object(world, "running", return_value=True), patch.object(value, "client_alive", return_value=True), \
             patch.object(world, "native_status", return_value={"SessionId": "owned-session", "Players": 0}), \
             patch.object(store, "preferences", return_value={"reconnect_grace": 30}), \
             patch.object(session.time, "monotonic", return_value=131):
            value.monitor()
        self.assertEqual(value.dispatch.call_args.args[0]["action"], "quit")

    def test_stale_native_ready_marker_is_not_accepted(self):
        value = self.supervisor()
        with patch.object(world, "running", return_value=True), patch.object(value, "client_alive", return_value=True), \
             patch.object(world, "native_status", return_value={"SessionId": "previous-session", "Players": 1, "Ready": 1}):
            value.monitor()
        self.assertFalse(value.seen_connection)
        value.publish.assert_not_called()

    def test_pid_reuse_does_not_claim_an_unrelated_game(self):
        value = self.supervisor()
        value.client = {"pid": 123, "created": 1, "executable": "DayZ_x64.exe"}
        with patch.object(world, "process_identity", return_value={"pid": 123, "created": 2, "executable": "DayZ_x64.exe"}):
            self.assertFalse(value.client_alive())

    def test_recovered_session_commands_follow_client_acknowledgement(self):
        value = self.supervisor()
        value.command_sequence = 0
        with patch.object(value, "client_status", return_value={"Acknowledged": 17}), \
                patch.object(store, "atomic_json") as write:
            value.command_client("quit")
        self.assertEqual(write.call_args.args[1]["Sequence"], 18)

    def test_unconnected_client_does_not_keep_server_running_forever(self):
        value = self.supervisor()
        value.connection_deadline = 100
        with patch.object(world, "running", return_value=True), patch.object(value, "client_alive", return_value=True), \
                patch.object(world, "native_status", return_value={"SessionId": "owned-session", "Players": 0}), \
                patch.object(session.time, "monotonic", return_value=101):
            value.monitor()
        self.assertEqual(value.dispatch.call_args.args[0]["action"], "quit")
        self.assertIn("did not connect", value.dispatch.call_args.args[0]["failure"])

    def test_login_failure_saves_and_stops_without_waiting_for_connection_deadline(self):
        value = self.supervisor()
        value.connection_deadline = 1000
        with patch.object(world, "running", return_value=True), patch.object(value, "client_alive", return_value=True), \
                patch.object(world, "native_status", return_value={"SessionId": "owned-session", "Players": 0}), \
                patch.object(value, "client_status", return_value={"Error": "Steam login unavailable"}):
            value.monitor()
        command = value.dispatch.call_args.args[0]
        self.assertEqual(command['action'], 'quit')
        self.assertEqual(command['failure'], 'Steam login unavailable')


if __name__ == "__main__": unittest.main()
