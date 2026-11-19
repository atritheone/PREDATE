"""Real packaged-backend recovery, backup restore and fresh-operation checks.

Only the exact owned server of a disposable validation world is interrupted.
Original worlds and snapshots are never replaced.
"""
from pathlib import Path
import json
import os
import signal
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from engine_sessions import Bridge, fixture
import platform_win
import session
import session_store as store
import world


def hide_validation(profile):
    info = store.metadata(profile)
    info['kind'] = 'validation'
    store.atomic_json(store.world_path(profile)/'operation.json', info)


def main():
    assert not platform_win.find_processes('DayZ_x64.exe') and not platform_win.find_processes('DayZServer_x64.exe')
    preferences = store.preferences()
    profile = fixture('crashlab-primitive-send-d1612')
    bridge = Bridge()
    report = {'profile': profile, 'passed': False}
    try:
        bridge.wait_action(bridge.action('continue', profile=profile))
        bridge.wait_phase({'playing'})
        before = json.loads((world.WORLD/'last-session.json').read_text())
        store.validate_snapshot(store.ROOT/'backups'/before['backup'])
        identity = json.loads(world.STATE.read_text())
        assert world.process_identity(identity['pid']) == {k: identity[k] for k in ('pid', 'created', 'executable')}
        assert world.WORLD == store.world_path(profile) and store.metadata(profile)['kind'] == 'validation'
        os.kill(identity['pid'], signal.SIGTERM)
        deadline = time.monotonic() + 40
        while time.monotonic() < deadline:
            status = bridge.call('status')
            if status['phase'] == 'error' and not platform_win.find_processes('DayZ_x64.exe'): break
            time.sleep(.25)
        assert status['phase'] == 'error'
        assert not platform_win.find_processes('DayZ_x64.exe')
        assert not json.loads((world.WORLD/'last-session.json').read_text())['clean']
        report['unexpected_server_exit_detected'] = True
        report['orphan_client_closed_normally'] = True
        bridge.wait_action(bridge.action('continue', profile=profile))
        recovered = bridge.call('status')['profile']
        hide_validation(recovered)
        world.select_profile(recovered)
        assert recovered != profile
        assert store.metadata(recovered)['restored_from'] == before['backup']
        bridge.wait_phase({'playing'})
        assert world.native_status()['BotHealth'] > 0
        report['recovered_bot_magazine'] = world.native_status()['BotMagazine']
        bridge.wait_action(bridge.action('quit'))
        bridge.wait_phase({'suspended'})
        report['unclean_session_recovers_separate_snapshot'] = True
        backup = json.loads((store.world_path(recovered)/'last-session.json').read_text())['backup']
        expected = store.validate_snapshot(store.ROOT/'backups'/backup)['files']
        bridge.wait_action(bridge.action('restore', backup=backup, name='Restore validation'))
        restored = bridge.call('status')['profile']
        hide_validation(restored)
        assert restored not in (profile, recovered)
        for relative, digest in expected.items():
            assert store.digest(store.world_path(restored)/relative) == digest, relative
        report['backup_restore_matches_every_hash'] = True
        print('PASS: unexpected-exit recovery and independent backup restore', flush=True)
        bridge.wait_action(bridge.action('new', name='Fresh operation validation'))
        fresh = bridge.call('status')['profile']
        hide_validation(fresh)
        world.select_profile(fresh)
        bridge.wait_phase({'playing'})
        progress = json.loads((world.PROFILE/'PredateProgress.json').read_text())
        assert not progress['Selected'] and not progress['Defeated']
        assert progress['Operator']['Deployment'] == 1
        assert world.native_status()['BotMagazine'] != 17
        bridge.wait_action(bridge.action('quit'))
        bridge.wait_phase({'suspended'})
        report['fresh_operation_starts_independently'] = True
        report['passed'] = True
    finally:
        try:
            if world.running() or platform_win.find_processes('DayZ_x64.exe'):
                bridge.wait_action(bridge.action('quit'), 90)
        finally:
            try: bridge.close()
            finally:
                store.save_preferences({k: v for k, v in preferences.items() if k != 'schema'})
                store.atomic_json(store.ROOT/'build/engine-recovery-results.json', report)
                print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
