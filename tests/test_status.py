"""Supervisor status must remain usable across transient filesystem misses."""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import session


class StatusTests(unittest.TestCase):
    def setUp(self):
        self.cache = session._status_cache
        session._status_cache = None
        self.owner = dict(pid=100, created=1234, executable='owned-supervisor.exe')
        self.status = dict(phase='paused', message='Operation paused', profile='validation-only')
        self.files = {session.MANAGER:self.owner, session.STATUS:self.status}
        self.read = patch.object(session, 'read_native', side_effect=lambda path:self.files.get(path, {}))
        self.identity = patch.object(session.world, 'process_identity', return_value=self.owner)
        self.prefs = patch.object(session.store, 'preferences', return_value=dict(selected_world='selected-operation'))
        self.read.start(); self.identity.start(); self.prefs.start()

    def tearDown(self):
        self.read.stop(); self.identity.stop(); self.prefs.stop()
        session._status_cache = self.cache

    def test_unreadable_status_keeps_last_paused_state_without_aliasing(self):
        first = session.state()
        first['phase'] = 'mutated-by-caller'
        self.files[session.STATUS] = {}
        self.assertEqual(session.state(), self.status)

    def test_transient_manager_read_revalidates_live_owner(self):
        session.state()
        self.files.clear()
        self.assertEqual(session.state(), self.status)

    def test_first_status_miss_returns_a_busy_state(self):
        self.files[session.STATUS] = {}
        self.assertEqual(session.state()['phase'], 'connecting')

    def test_dead_owner_clears_cache_and_returns_idle(self):
        session.state()
        session.world.process_identity.return_value = None
        self.assertEqual(session.state()['phase'], 'idle')
        self.assertIsNone(session._status_cache)

    def test_new_owner_cannot_inherit_old_paused_status(self):
        session.state()
        new_owner = self.owner | dict(created=9999)
        self.files[session.MANAGER] = new_owner
        self.files[session.STATUS] = {}
        session.world.process_identity.return_value = new_owner
        self.assertEqual(session.state()['phase'], 'connecting')

    def test_fresh_resumed_snapshot_replaces_cached_pause(self):
        session.state()
        self.files[session.STATUS] = dict(phase='playing', message='Playing')
        self.assertEqual(session.state()['phase'], 'playing')

    def test_snapshot_from_before_live_owner_started_is_not_current(self):
        owner = self.owner | dict(created=116444736000000000 + 200 * 10000000)
        self.files[session.MANAGER] = owner
        session.world.process_identity.return_value = owner
        self.files[session.STATUS] = self.status | dict(updated=100)
        self.assertEqual(session.state()['phase'], 'connecting')


if __name__ == '__main__': unittest.main()
