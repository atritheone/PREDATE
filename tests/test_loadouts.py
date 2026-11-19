import sys
"""Starting-kit selection affects new worlds and preserves restored inventory rules."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import backend_entry
import loadouts
import session_store as store


class StartingKitTests(unittest.TestCase):
    def test_selected_kit_is_saved_for_the_new_world(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(store, 'ROOT', Path(directory)), \
             patch('world.select_profile'), patch('world.setup'):
            info = store.create_operation('AK operation', profile='op-kit', extra_mods=[], player_loadout='ak74',
                                          opponent_count=3, footprints=True)
            config = store.read_json(store.world_path(info['id'])/'profile/PredateConfig.json')
            self.assertEqual(config['PlayerLoadout'], 'ak74')
            self.assertEqual(config['OpponentCount'], 3)
            self.assertTrue(config['EnableFootprints'])
            self.assertTrue(config['EnableStrategicHunter'])

    def test_unknown_kit_cannot_create_or_start_a_world(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(store, 'ROOT', Path(directory)), \
             patch('world.setup') as setup:
            with self.assertRaisesRegex(ValueError, 'Unknown starting kit'):
                store.create_operation('Invalid operation', profile='op-invalid', extra_mods=[], player_loadout='unknown')
            setup.assert_not_called()
            self.assertFalse(store.world_path('op-invalid').exists())

    def test_backup_restore_preserves_the_saved_kit_config_exactly(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(store, 'ROOT', Path(directory)), \
             patch('world.select_profile'), patch('world.setup'), patch('world.running', return_value=False):
            info = store.create_operation('AK operation', profile='op-kit', extra_mods=[], player_loadout='ak74')
            config_path = store.world_path(info['id'])/'profile/PredateConfig.json'
            config = config_path.read_bytes()
            snapshot = store.snapshot(info['id'])
            restored = store.restore_snapshot(snapshot.name, 'Restored AK operation')
            self.assertEqual((store.world_path(restored['id'])/'profile/PredateConfig.json').read_bytes(), config)

    def test_kit_cannot_be_changed_through_continue_or_invalid_new_action(self):
        with patch('session.request') as request:
            with self.assertRaisesRegex(ValueError, 'only to a new operation'):
                backend_entry.dispatch('action', {'action':'continue', 'profile':'op-fixture', 'loadout':'ak74'})
            with self.assertRaisesRegex(ValueError, 'Unknown starting kit'):
                backend_entry.dispatch('action', {'action':'new', 'name':'Fixture', 'loadout':'unknown'})
            request.assert_not_called()

    def test_options_expose_game_choices_without_native_implementation_fields(self):
        options = loadouts.options()
        self.assertEqual({kit['id'] for kit in options}, {'m4a1', 'ak74'})
        self.assertTrue(all(set(kit) == {'id','name','description'} for kit in options))

    def test_opponent_count_only_applies_to_new_operations(self):
        with patch('session.request', return_value='request') as request:
            with self.assertRaisesRegex(ValueError, 'only to a new operation'):
                backend_entry.dispatch('action', {'action':'continue', 'profile':'op-fixture', 'opponents':3})
            for invalid in (True, '3', 0, 2, 7, 31):
                with self.assertRaisesRegex(ValueError, '1, 3, 6, 15, or 30'):
                    backend_entry.dispatch('action', {'action':'new', 'name':'Fixture', 'opponents':invalid})
            request.assert_not_called()
            backend_entry.dispatch('action', {'action':'new', 'name':'Fixture', 'opponents':30})
            request.assert_called_once_with('new', name='Fixture', opponents=30)

    def test_footprint_clues_only_apply_to_new_operations(self):
        with patch('session.request', return_value='request') as request:
            with self.assertRaisesRegex(ValueError, 'only to a new operation'):
                backend_entry.dispatch('action', {'action':'continue', 'profile':'op-fixture', 'footprints':True})
            with self.assertRaisesRegex(ValueError, 'true or false'):
                backend_entry.dispatch('action', {'action':'new', 'name':'Fixture', 'footprints':1})
            request.assert_not_called()
            backend_entry.dispatch('action', {'action':'new', 'name':'Fixture', 'footprints':True})
            request.assert_called_once_with('new', name='Fixture', footprints=True)


if __name__ == '__main__': unittest.main()
