import sys
"""New hunts opt into the tested controller; restores keep their original rules."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import session_store as store
import spawn_points


class OperationHuntDefaultsTests(unittest.TestCase):
    def test_only_new_hunts_receive_active_hunter_defaults(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with patch.object(store, 'ROOT', root), patch('world.select_profile'), patch('world.setup'):
                info = store.create_operation('New hunt', profile='op-new', extra_mods=[])
                config = store.read_json(store.world_path(info['id'])/'profile/PredateConfig.json')
                self.assertTrue(config['EnableFairInformation'])
                self.assertTrue(config['EnableSharedSignals'])
                self.assertTrue(config['EnableStrategicHunter'])
                restored = store.create_operation('Restore staging', profile='op-old', extra_mods=[], hunt_defaults=False)
                self.assertFalse((store.world_path(restored['id'])/'profile/PredateConfig.json').exists())

    def test_custom_spawn_is_saved_exactly_and_invalid_input_creates_no_world(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(store, 'ROOT', Path(temporary)), \
             patch('world.select_profile'), patch('world.setup') as setup:
            with self.assertRaisesRegex(ValueError, 'coordinates'):
                store.create_operation('Invalid', profile='op-invalid', extra_mods=[], spawn={'x': float('nan'), 'z': 6500})
            setup.assert_not_called()
            store.create_operation('Custom', profile='op-custom', extra_mods=[], opponent_count=6,
                                   spawn={'x': 4500.25, 'z': 6501.75})
            config = store.read_json(store.world_path('op-custom')/'profile/PredateConfig.json')
            self.assertEqual(config['PlayerStart'], [4500.25, 0, 6501.75])
            self.assertTrue(config['ExplicitPlayerStart'])
            self.assertEqual(config['OpponentCount'], 6)
            self.assertEqual(spawn_points.config({'preset': 'original'}), {})
            self.assertEqual(spawn_points.config({'preset': 'cherno'})['PlayerStart'], [6063, 0, 1932])

    def test_regional_selection_balances_regions_before_selecting_a_point(self):
        for region in spawn_points.REGIONS:
            with patch('spawn_points.random.choice', side_effect=[region, region[-1]]) as choice:
                config = spawn_points.config({'preset': 'regional'})
                _, east, north = spawn_points.PRESETS[region[-1]]
                self.assertEqual(config, {'PlayerStart': [east, 0, north], 'ExplicitPlayerStart': True})
                self.assertEqual(choice.call_args_list[0].args[0], spawn_points.REGIONS)


if __name__ == '__main__':
    unittest.main()
