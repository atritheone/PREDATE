import sys
from pathlib import Path
import json
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import world


class CleanShutdownRecoveryTests(unittest.TestCase):
    def run_stop(self, status, actual=None):
        identity = dict(pid=123, created=456, executable=str(world.SERVER/'DayZServer_x64.exe'), session_id='owned')
        expected = {key:identity[key] for key in ('pid', 'created', 'executable')}
        state = Mock()
        state.read_text.return_value = json.dumps(identity)
        with patch.object(world, 'STATE', state), \
             patch.object(world, 'running', side_effect=[True, True, False, False]), \
             patch.object(world, 'control', return_value=42), \
             patch.object(world, 'native_status', return_value=status), \
             patch.object(world, 'process_identity', return_value=expected if actual is None else actual), \
             patch.object(world.time, 'monotonic', side_effect=[0, 0, 16, 17]), \
             patch.object(world.time, 'sleep'), \
             patch.object(world, 'rcon') as rcon:
            world.stop(make_backup=False)
            return rcon

    def test_saved_stop_ack_allows_one_native_clean_shutdown_fallback(self):
        rcon = self.run_stop(dict(SessionId='owned', Acknowledged=42, Phase='stopping'))
        rcon.assert_called_once_with('#shutdown', timeout=5)

    def test_unsaved_stop_request_never_uses_fallback(self):
        rcon = self.run_stop(dict(SessionId='owned', Acknowledged=41, Phase='paused'))
        rcon.assert_not_called()

    def test_another_sessions_ack_cannot_authorize_shutdown(self):
        rcon = self.run_stop(dict(SessionId='foreign', Acknowledged=99, Phase='stopping'))
        rcon.assert_not_called()

    def test_reused_process_identity_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'changed'):
            self.run_stop(dict(SessionId='owned', Acknowledged=42, Phase='stopping'), actual={'pid':123, 'created':999})


if __name__ == '__main__': unittest.main()
