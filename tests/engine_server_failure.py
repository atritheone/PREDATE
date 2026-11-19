"""Controlled runtime-failure recovery in a disposable, connected session."""
from pathlib import Path
import argparse
import json
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import Bridge, fixture, native_evidence
import platform_win
import session
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packaged', action='store_true')
    args = parser.parse_args()
    assert not any(platform_win.find_processes(name) for name in ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe'))
    preferences = store.preferences()
    profile = fixture('crashlab-primitive-send-d1612')
    entry = world.MISSION/'init.c'
    script = '''
class PredateRuntimeFailureMission : PredateValidationMission
{
    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(WaitForFailureFixture, 500, false);
    }
    void WaitForFailureFixture()
    {
        if (!m_PredateSession.m_ActorsActive)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(WaitForFailureFixture, 500, false);
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ControlledFailure, 5000, false);
    }
    void ControlledFailure()
    {
        Error("PREDATE controlled runtime failure fixture");
    }
}
'''
    marker = 'Mission CreateCustomMission(string path)'
    entry.write_text(entry.read_text().replace(marker, script+'\n'+marker).replace('return new PredateValidationMission;', 'return new PredateRuntimeFailureMission;'))
    report = dict(profile=profile, passed=False, packaged=args.packaged)
    report.update(native_evidence())
    bridge = Bridge(source=not args.packaged)
    try:
        bridge.wait_action(bridge.action('continue', profile=profile))
        owner = session.read_native(session.MANAGER)
        report['supervisor_executable'] = owner['executable']
        if args.packaged:
            report['backend_sha256'] = store.digest(Path(owner['executable']))
            assert report['backend_sha256'] == store.digest(ROOT/'launcher-app/backend/PREDATEBackend.exe')
        bridge.wait_phase({'playing'})
        print('Connected; waiting for the controlled runtime failure', flush=True)
        deadline = time.monotonic()+35
        failure = None
        while time.monotonic()<deadline:
            failure = session.read_native(world.PROFILE/'PredateFailure.json')
            if failure and not world.running() and not platform_win.find_processes('DayZ_x64.exe') and not (session.RUNTIME/'active-session.json').exists(): break
            time.sleep(.1)
        assert failure and failure['kind'] == 'runtime', failure
        assert 'controlled runtime failure fixture' in failure['excerpt'], failure.get('message')
        assert failure.get('closed_at'), failure
        report['server_close_seconds'] = round(failure['closed_at']-failure['detected_at'], 3)
        report['whole_cleanup_seconds'] = round(time.time()-failure['detected_at'], 3)
        assert report['server_close_seconds'] < 2
        assert report['whole_cleanup_seconds'] < 15
        assert not world.running() and not platform_win.find_processes('DayZ_x64.exe')
        assert not (session.RUNTIME/'active-session.json').exists()
        last = store.read_json(world.WORLD/'last-session.json')
        assert last['clean'] is False, last
        snapshot = ROOT/'backups'/last['backup']
        store.validate_snapshot(snapshot)
        assert len(list((ROOT/'backups').glob(f'{profile}-session-*.zip'))) == 1
        report.update(passed=True, crashed_stores_not_validated=True, prior_snapshot_retained=True,
                      disconnected_client_closed=True)
    finally:
        try:
            if world.running() or platform_win.find_processes('DayZ_x64.exe'):
                bridge.wait_action(bridge.action('quit'), 30)
        finally:
            bridge.close()
            store.save_preferences({key:value for key,value in preferences.items() if key != 'schema'})
            store.atomic_json(ROOT/'build/engine-server-failure-results.json', report)
            store.atomic_json(ROOT/f'build/engine-server-failure-{profile}.json', report)
            print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
