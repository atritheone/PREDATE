import sys
"""A failed server cannot acknowledge a save or authorize a clean snapshot."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import session
import session_store as store
import world


class FailedServerCleanupTests(unittest.TestCase):
    def supervisor(self):
        value = session.Supervisor()
        value.profile = 'op-fixture'
        value.session_id = 'owned'
        value.configure = Mock()
        value.publish = Mock()
        return value

    def test_quit_after_server_exit_does_not_wait_for_an_impossible_ack(self):
        value = self.supervisor()
        value.command_client = Mock()
        value.stop_server = Mock()
        with patch.object(world, 'running', return_value=False), patch.object(session.world_pause, 'resume'), \
             patch.object(value, 'client_alive', side_effect=[True, False, False]), \
             patch.object(session.time, 'monotonic', return_value=0):
            value.save_quit()
        value.command_client.assert_called_once_with('server-closed')
        value.stop_server.assert_called_once_with('save-and-quit')

    def test_server_exit_during_save_switches_to_client_exit(self):
        value = self.supervisor()
        value.command_client = Mock()
        value.stop_server = Mock()
        value.client_status = Mock(return_value={'Acknowledged':999, 'Error':'old timeout'})
        with patch.object(world, 'running', side_effect=[True, False]), patch.object(session.world_pause, 'resume'), \
             patch.object(value, 'client_alive', side_effect=[True, True, False, False]), \
             patch.object(session.time, 'monotonic', return_value=0), patch.object(session.time, 'sleep'):
            value.save_quit()
        self.assertEqual([call.args[0] for call in value.command_client.call_args_list], ['quit', 'server-closed'])

    def test_error_phase_still_closes_the_disconnected_client_once(self):
        value = self.supervisor()
        value.data['phase'] = 'error'
        value.command_client = Mock()
        with patch.object(world, 'running', return_value=False), patch.object(value, 'client_alive', return_value=True):
            value.monitor()
            value.monitor()
        value.command_client.assert_called_once_with('server-closed')

    def test_runtime_failure_does_not_become_clean_when_guard_requested_stop(self):
        value = self.supervisor()
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(session, 'RUNTIME', Path(directory)), patch.object(session.world_pause, 'resume'), \
                 patch.object(world, 'running', return_value=False), \
                 patch.object(session, 'read_native', return_value={'session_id':'owned'}), \
                 patch.object(world, 'process_failure', return_value={'kind':'runtime'}), \
                 patch.object(world, 'native_status', return_value={'SessionId':'owned', 'Phase':'stopping'}), \
                 patch.object(store, 'snapshot') as snapshot, patch.object(store, 'atomic_json') as write:
                value.stop_server('failed-fixture')
        snapshot.assert_not_called()
        write.assert_not_called()
        self.assertIsNone(value.session_id)
        self.assertEqual(value.publish.call_args.args[0], 'error')

    def test_repeated_quit_after_failed_cleanup_does_not_validate_damaged_stores(self):
        value = self.supervisor()
        value.session_id = None
        value.stop_server = Mock()
        value.save_quit()
        value.stop_server.assert_not_called()

    def test_acknowledged_ordinary_stop_still_creates_a_validated_snapshot(self):
        value = self.supervisor()
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(session, 'RUNTIME', Path(directory)), patch.object(session.world_pause, 'resume'), \
                 patch.object(world, 'running', return_value=False), \
                 patch.object(session, 'read_native', return_value={'session_id':'owned'}), \
                 patch.object(world, 'process_failure', return_value=None), \
                 patch.object(world, 'native_status', return_value={'SessionId':'owned', 'Phase':'stopping'}) as read_status, \
                 patch.object(store, 'snapshot', return_value=Path(directory)/'validated.zip') as snapshot, \
                 patch.object(store, 'atomic_json') as write:
                value.stop_server('ordinary-stop')
        read_status.assert_called_once_with()
        snapshot.assert_called_once_with('op-fixture', 'owned', 'ordinary-stop')
        self.assertTrue(write.call_args.args[1]['clean'])
        self.assertEqual(value.publish.call_args.args[0], 'suspended')


if __name__ == '__main__': unittest.main()
