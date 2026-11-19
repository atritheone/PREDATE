import sys
"""Changed optional content is rejected before any saved world is opened."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import mod_presets
import session
import session_store as store


class ModPresetTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.mod = self.root/'extra'
        (self.mod/'addons').mkdir(parents=True)
        self.pbo = self.mod/'addons/extra.pbo'
        self.pbo.write_bytes(b'original engine content')
        self.mods = [str(self.mod)]

    def tearDown(self):
        self.directory.cleanup()

    def test_same_filename_changed_content_is_rejected(self):
        recorded = mod_presets.capture(self.mods)
        self.pbo.write_bytes(b'changed engine content!')
        with self.assertRaisesRegex(ValueError, 'extra.pbo'):
            mod_presets.verify(self.mods, recorded)

    def test_added_and_removed_runtime_files_are_rejected(self):
        extra = self.mod/'addons/second.pbo'
        recorded = mod_presets.capture(self.mods)
        extra.write_bytes(b'new content')
        with self.assertRaisesRegex(ValueError, 'second.pbo'):
            mod_presets.verify(self.mods, recorded)
        recorded = mod_presets.capture(self.mods)
        extra.unlink()
        with self.assertRaisesRegex(ValueError, 'second.pbo'):
            mod_presets.verify(self.mods, recorded)

    def test_documentation_changes_do_not_change_the_preset(self):
        recorded = mod_presets.capture(self.mods)
        (self.mod/'readme.txt').write_text('new documentation')
        self.assertEqual(mod_presets.verify(self.mods, recorded), recorded)

    def test_duplicate_paths_and_argument_delimiters_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'more than once'):
            mod_presets.capture([*self.mods, *self.mods])
        with self.assertRaisesRegex(ValueError, 'semicolon'):
            mod_presets.capture([str(self.root/'one;two')])

    def test_legacy_operation_records_its_first_verified_preset_once(self):
        with patch.object(store, 'ROOT', self.root):
            path = store.world_path('op-fixture')/'operation.json'
            store.atomic_json(path, {'schema':1, 'id':'op-fixture', 'name':'Fixture', 'extra_mods':self.mods, 'kind':'operation'})
            first = store.verify_operation_mods('op-fixture')
            saved = path.read_bytes()
            self.assertEqual(store.verify_operation_mods('op-fixture'), first)
            self.assertEqual(path.read_bytes(), saved)
            self.pbo.write_bytes(b'updated')
            with self.assertRaisesRegex(ValueError, 'not opened'):
                store.verify_operation_mods('op-fixture')
            self.assertEqual(path.read_bytes(), saved)

    def test_supervisor_rejects_changes_before_loading_keys_or_starting(self):
        with patch.object(store, 'ROOT', self.root):
            path = store.world_path('op-fixture')/'operation.json'
            store.atomic_json(path, {'schema':1, 'id':'op-fixture', 'name':'Fixture', 'extra_mods':self.mods, 'kind':'operation', 'mod_lock':mod_presets.capture(self.mods)})
            self.pbo.write_bytes(b'updated')
            with patch('world.select_profile'), patch('mod_presets.install_keys') as copy:
                with self.assertRaisesRegex(ValueError, 'Mod content changed'):
                    session.Supervisor().configure('op-fixture')
                copy.assert_not_called()

    def test_reused_signing_key_filename_does_not_replace_a_standard_key(self):
        keys = self.root/'runtime/keys'
        keys.mkdir(parents=True)
        standard = keys/'shared.bikey'
        standard.write_bytes(b'standard key')
        extra = self.mod/'keys/shared.bikey'
        extra.parent.mkdir()
        extra.write_bytes(b'additional key')
        verified = mod_presets.capture(self.mods)
        mod_presets.install_keys(verified, keys)
        mod_presets.install_keys(verified, keys)
        self.assertEqual(standard.read_bytes(), b'standard key')
        installed = list(keys.glob('PredateExtra-*.bikey'))
        self.assertEqual(len(installed), 1)
        self.assertEqual(installed[0].read_bytes(), extra.read_bytes())

    def test_key_changed_after_verification_is_rejected_before_any_key_is_added(self):
        extra = self.mod/'keys/shared.bikey'
        extra.parent.mkdir()
        extra.write_bytes(b'original key')
        verified = mod_presets.capture(self.mods)
        extra.write_bytes(b'changed key')
        keys = self.root/'runtime/keys'
        with self.assertRaisesRegex(ValueError, 'after verification'):
            mod_presets.install_keys(verified, keys)
        self.assertFalse(keys.exists())

    def test_new_operation_records_mod_content(self):
        with patch.object(store, 'ROOT', self.root), patch('world.select_profile'), patch('world.setup'):
            info = store.create_operation('New operation', profile='op-fixture', extra_mods=self.mods)
            self.assertEqual(info['mod_lock'], mod_presets.capture(self.mods))

    def test_snapshot_retains_the_active_profiles_verified_mod_list(self):
        with patch.object(store, 'ROOT', self.root), patch('world.select_profile') as select, \
             patch('world.setup'), patch('world.running', return_value=False):
            info = store.create_operation('Modded operation', profile='op-fixture', extra_mods=self.mods)
            select.reset_mock()
            with patch('world.PROFILE_NAME', info['id']), patch('world.MODS', [self.mod]) as prepared:
                store.snapshot(info['id'], reason='before-continue')
                self.assertEqual(prepared, [self.mod])
            select.assert_not_called()

    def test_restore_keeps_recorded_content_and_rejects_updates_before_creating_a_world(self):
        with patch.object(store, 'ROOT', self.root), patch('world.select_profile'), patch('world.setup'), patch('world.running', return_value=False):
            info = store.create_operation('Original', profile='op-fixture', extra_mods=self.mods)
            progress = store.world_path(info['id'])/'profile/PredateProgress.json'
            progress.write_text('{"Selected":1,"PulseRemaining":38}')
            snapshot = store.snapshot(info['id'])
            before = sorted(path.name for path in (self.root/'worlds').iterdir())
            original = self.pbo.read_bytes()
            self.pbo.write_bytes(b'updated')
            with self.assertRaisesRegex(ValueError, 'Mod content changed'):
                store.restore_snapshot(snapshot.name, 'Unsafe restore')
            self.assertEqual(sorted(path.name for path in (self.root/'worlds').iterdir()), before)
            self.pbo.write_bytes(original)
            restored = store.restore_snapshot(snapshot.name, 'Verified restore')
            self.assertEqual(restored['mod_lock'], info['mod_lock'])
            self.assertEqual((store.world_path(restored['id'])/'profile/PredateProgress.json').read_bytes(), progress.read_bytes())

    def test_standard_mod_cannot_be_loaded_twice(self):
        with patch.object(store, 'ROOT', self.root):
            core = self.root/'mods/@Predate/addons'
            core.mkdir(parents=True)
            (core/'Predate.pbo').write_bytes(b'fixture')
            with self.assertRaisesRegex(ValueError, 'already loads'):
                store.validate_mods([str(core.parent)])


if __name__ == '__main__':
    unittest.main()
