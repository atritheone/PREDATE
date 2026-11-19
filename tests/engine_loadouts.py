"""Fresh native kit and ammunition-preserving continuation, in a disposable world."""
from pathlib import Path
import argparse
import json
import os
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import native_evidence
import loadouts
import platform_win
import session
import session_store as store
import world


def close_client(nonce):
    if not platform_win.find_processes('DayZ_x64.exe'): return
    status = session.read_native(world.WORLD/'client/PredateClientStatus.json')
    action = 'quit' if world.running() else 'server-closed'
    store.atomic_json(world.WORLD/'client/PredateClientCommand.json', {
        'Schema':1, 'SessionId':nonce, 'Sequence':status.get('Acknowledged',0)+1, 'Action':action})
    deadline = time.monotonic()+30
    while platform_win.find_processes('DayZ_x64.exe') and time.monotonic()<deadline: time.sleep(.1)
    assert not platform_win.find_processes('DayZ_x64.exe'), 'Owned kit-validation client did not close'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kit', choices=[kit['Id'] for kit in loadouts.catalog()], required=True)
    args = parser.parse_args()
    assert not any(platform_win.find_processes(name) for name in ('DayZ_x64.exe','DayZServer_x64.exe','DayZDiag_x64.exe'))
    preferences = store.preferences()
    original_addon = store.digest(ROOT/'mods/@Predate/addons/Predate.pbo')
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = 'validation-kit-'+uuid.uuid4().hex[:6]
    info = store.create_operation('Kit validation', profile=profile, extra_mods=[], player_loadout=args.kit)
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json', info)
    entry = world.MISSION/'init.c'
    probe = (ROOT/'source/validation/loadout_probe.c').read_text()
    entry.write_text(entry.read_text().replace('Mission CreateCustomMission(string path)', probe+'\nMission CreateCustomMission(string path)').replace('return new MissionServer;', 'return new PredateKitProbeMission;'))
    report = dict(profile=profile, kit=args.kit, candidate=True, passed=False, test_ai_simulation_held=True)
    report.update(native_evidence())
    nonce = ''
    try:
        for filename in ('KitInitial.json','KitReopened.json'):
            world.start()
            nonce = store.read_json(world.STATE)['session_id']
            world.launch_client(nonce, extra_args=['-window','-x=1920','-y=1080'])
            path = world.PROFILE/filename
            deadline = time.monotonic()+150
            while not path.exists() and time.monotonic()<deadline:
                assert world.running(), 'Kit-validation server exited'
                failure = session.read_native(world.WORLD/'client/PredateFailure.json')
                assert not failure, failure.get('message')
                time.sleep(.1)
            data = store.read_json(path, {})
            report[filename.removesuffix('.json')] = data
            assert data.get('Checks',0) >= 10 and data['Failures'] == 0, data
            assert data['Kit'] == args.kit
            close_client(nonce)
            world.stop(make_backup=False)
        report['passed'] = True
    finally:
        close_client(nonce)
        if world.running(): world.stop(make_backup=False)
        report['playable_addon_unchanged'] = store.digest(ROOT/'mods/@Predate/addons/Predate.pbo') == original_addon
        report['user_preferences_unchanged'] = store.preferences() == preferences
        store.atomic_json(ROOT/f'build/engine-loadout-{args.kit}-results.json', report)
        store.atomic_json(ROOT/f'build/engine-loadout-{profile}.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
