import sys
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import session_store as store


class AtomicJsonSharingTests(unittest.TestCase):
    def test_transient_reader_lock_retains_old_data_until_replace_succeeds(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'status.json'
            path.write_text('{"phase":"paused"}')
            replace = store.os.replace
            attempts = []
            def locked_once(source, destination):
                attempts.append(source)
                self.assertEqual(json.loads(path.read_text()), {'phase':'paused'})
                if len(attempts) == 1: raise PermissionError('reader still has the file open')
                replace(source, destination)
            with patch.object(store.os, 'replace', side_effect=locked_once), patch.object(store.time, 'sleep') as sleep:
                store.atomic_json(path, {'phase':'playing'})
            self.assertEqual(json.loads(path.read_text()), {'phase':'playing'})
            self.assertEqual(attempts[0], attempts[1])
            sleep.assert_called_once_with(.01)
            self.assertEqual(list(Path(folder).glob('*.tmp')), [])

    def test_permanent_denial_is_bounded_and_preserves_the_previous_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'state.json'
            path.write_text('{"generation":1}')
            with patch.object(store.os, 'replace', side_effect=PermissionError('denied')) as replace, patch.object(store.time, 'sleep'):
                with self.assertRaises(PermissionError): store.atomic_json(path, {'generation':2})
            self.assertEqual(replace.call_count, 6)
            self.assertEqual(json.loads(path.read_text()), {'generation':1})
            self.assertEqual(list(Path(folder).glob('*.tmp')), [])

    def test_other_io_failures_are_reported_without_retries(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'state.json'
            with patch.object(store.os, 'replace', side_effect=OSError('disk unavailable')) as replace, patch.object(store.time, 'sleep') as sleep:
                with self.assertRaises(OSError): store.atomic_json(path, {'generation':2})
            replace.assert_called_once()
            sleep.assert_not_called()


if __name__ == '__main__': unittest.main()
