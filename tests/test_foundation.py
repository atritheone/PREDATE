"""Fast safety checks with temporary worlds; no DayZ process or live save is touched."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import os
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import world
import release


class FoundationTests(unittest.TestCase):
    def tearDown(self):
        world.select_profile("main")

    def test_profiles_isolate_all_state_credentials_and_ports(self):
        main = {name: getattr(world, name) for name in
                ("WORLD", "PROFILE", "STORAGE", "MISSION", "SETTINGS", "STATE", "CONFIG")}
        world.select_profile("development")
        for name, path in main.items():
            self.assertNotEqual(path, getattr(world, name), name)
            self.assertTrue(getattr(world, name).is_relative_to(world.ROOT / "worlds/development"))
        self.assertEqual(world.PORT, 2502)
        for invalid in ("../main", "C:\\world", "../development", "", "a/b"):
            with self.assertRaises(ValueError): world.select_profile(invalid)

    def test_test_launcher_refuses_normal_world(self):
        world.select_profile("main")
        with patch.object(world, "play") as play:
            with self.assertRaisesRegex(RuntimeError, "require.*development"):
                world.test()
            play.assert_not_called()

    def test_candidate_override_is_confined_to_validation_worlds(self):
        with patch.dict(os.environ, {'PREDATE_CANDIDATE_MOD':'1'}):
            for name in ('development','validation-knowledge'):
                world.select_profile(name)
                self.assertEqual(world.MODS[-1],world.ROOT/'mods/@PredateCandidate')
            for name in ('main','op-user-save'):
                world.select_profile(name)
                self.assertNotEqual(world.MODS[-1],world.ROOT/'mods/@PredateCandidate')

    def test_mission_update_preserves_native_and_operator_saves(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            template = root / "source/mission/template"
            template.mkdir(parents=True)
            (template / "cfgweather.xml").write_text("authored weather")
            (template.parent / "bootstrap.c").write_text("authored bootstrap")
            (template.parent / "factory.c").write_text("authored factory")
            (template.parent / "base-files.json").write_text("{}")
            mission = root / "world/mission"
            saved = mission / "storage_1/expansion/ai/raven/group.bin"
            saved.parent.mkdir(parents=True)
            saved.write_bytes(b"saved operator")
            (mission / "init.c").write_text("old mission")
            with patch.object(world, "ROOT", root), patch.object(world, "MISSION", mission), patch.object(world, "running", return_value=False):
                world.install_mission()
                self.assertEqual((mission / "init.c").read_text(), "authored bootstrap\n\nauthored factory")
                self.assertEqual((mission / "cfgweather.xml").read_text(), "authored weather")
                self.assertEqual(saved.read_bytes(), b"saved operator")
                first = (mission / "init.c").stat().st_mtime_ns
                world.install_mission()
                self.assertEqual((mission / "init.c").stat().st_mtime_ns, first)

    def test_lock_detects_dependency_drift(self):
        with tempfile.TemporaryDirectory() as temporary:
            lock = Path(temporary) / "release-lock.json"
            lock.write_text(json.dumps({"schema": 1, "dependencies": {"mod": "original"}}))
            with patch.object(release, "LOCK", lock), patch.object(release, "collect", return_value={"schema": 1, "dependencies": {"mod": "changed"}}):
                with self.assertRaisesRegex(RuntimeError, "dependencies"):
                    release.verify()


if __name__ == "__main__":
    unittest.main()
