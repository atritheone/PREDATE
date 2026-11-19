import sys
import ctypes
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import session
import session_store as store
import world
import world_pause


class PauseOwnershipTests(unittest.TestCase):
    def identity(self):
        return {'pid':123,'created':456,'executable':str(world.SERVER/'DayZServer_x64.exe'), 'session_id':'owned'}

    def test_pid_reuse_is_rejected_before_opening_a_process(self):
        identity=self.identity()
        actual={k:identity[k] for k in ('pid','created','executable')} | {'created':999}
        with patch.object(world,'process_identity',return_value=actual), patch.object(ctypes,'WinDLL') as api:
            with self.assertRaisesRegex(RuntimeError,'changed'):world_pause._change(identity,True)
        api.assert_not_called()

    def test_unrelated_executable_is_never_suspended(self):
        identity=self.identity() | {'executable':str(world.SERVER/'unrelated.exe')}
        with patch.object(ctypes,'WinDLL') as api:
            with self.assertRaisesRegex(RuntimeError,'Only this project'):world_pause._change(identity,True)
        api.assert_not_called()

    def test_recovery_resumes_owned_lease_once_and_removes_it(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'lease.json'
            identity=self.identity()
            store.atomic_json(path,{'session_id':'owned','server':identity})
            actual={k:identity[k] for k in ('pid','created','executable')}
            with patch.object(world_pause,'lease_path',return_value=path), patch.object(world,'process_identity',return_value=actual), patch.object(world_pause,'_change') as change:
                world_pause.resume('owned')
                world_pause.resume('owned')
            change.assert_called_once_with(identity,False)
            self.assertFalse(path.exists())

    def test_foreign_session_lease_is_retained(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'lease.json'
            store.atomic_json(path,{'session_id':'different','server':self.identity()})
            with patch.object(world_pause,'lease_path',return_value=path), patch.object(world_pause,'_change') as change:
                with self.assertRaisesRegex(RuntimeError,'Another session'):world_pause.resume('owned')
            self.assertTrue(path.exists())
            change.assert_not_called()

    def test_paused_disconnect_does_not_start_reconnect_grace(self):
        supervisor=session.Supervisor()
        supervisor.seen_connection=True
        supervisor.session_id='owned'
        supervisor.publish=Mock()
        with patch.object(world_pause,'suspended',return_value=True), patch.object(world,'control') as control:
            result=supervisor.monitor_pause({'Players':0,'Paused':True,'PauseReady':True},
                {'Paused':True,'PauseDisconnected':True,'Phase':'menu'})
        self.assertTrue(result)
        self.assertIsNone(supervisor.disconnected_at)
        self.assertEqual(supervisor.publish.call_args.args[0],'paused')
        control.assert_not_called()

    def test_partial_client_status_never_requests_resume(self):
        supervisor=session.Supervisor()
        supervisor.seen_connection=True
        with patch.object(world_pause,'resume') as resume, patch.object(world,'control') as control:
            self.assertTrue(supervisor.monitor_pause({'Paused':True,'PauseReady':True},{}))
        resume.assert_not_called()
        control.assert_not_called()

    def test_native_pause_records_ownership_without_suspending_a_process(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'lease.json'
            identity=self.identity()
            actual={k:identity[k] for k in ('pid','created','executable')}
            with patch.object(world_pause,'lease_path',return_value=path), \
                    patch.object(store,'read_json',side_effect=lambda p,*args: identity if p==world.STATE else {}), \
                    patch.object(world,'process_identity',return_value=actual), \
                    patch.object(world,'native_status',return_value={'SessionId':'owned','Paused':True,'PauseReady':True}), \
                    patch.object(world_pause,'_change') as change:
                world_pause.pause('owned')
                self.assertEqual(__import__('json').loads(path.read_text())['mode'],'gameplay-hold')
                change.assert_not_called()


if __name__=='__main__':unittest.main()
