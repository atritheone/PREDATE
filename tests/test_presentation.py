import sys
"""Display preferences are validated separately from operation saves."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import presentation
import session_store as store


class PresentationTests(unittest.TestCase):
    def test_legacy_settings_keep_the_approved_compass(self):
        self.assertEqual(presentation.validate({}), {'Schema':1,'CompassPercent':100,'HighContrast':False})

    def test_invalid_display_choice_does_not_replace_preferences(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(store,'ROOT',Path(directory)):
            original = store.save_preferences({})
            for value in (79,151,100.5,True,'100'):
                with self.assertRaisesRegex(ValueError,'Compass size'):
                    store.save_preferences({'receiver_scale':value})
                self.assertEqual(store.preferences(),original)
            with self.assertRaisesRegex(ValueError,'high contrast'):
                store.save_preferences({'receiver_high_contrast':1})
            self.assertEqual(store.preferences(),original)

    def test_client_options_are_not_written_into_operation_rules(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            server_config = root/'profile/PredateConfig.json'
            store.atomic_json(server_config,{'SchemaVersion':1,'PlayerLoadout':'ak74'})
            original = server_config.read_bytes()
            presentation.write_client_options(root/'client',{'receiver_scale':150,'receiver_high_contrast':True})
            self.assertEqual(store.read_json(root/'client/PredatePresentation.json'),
                             {'Schema':1,'CompassPercent':150,'HighContrast':True})
            self.assertEqual(server_config.read_bytes(),original)


if __name__ == '__main__': unittest.main()
