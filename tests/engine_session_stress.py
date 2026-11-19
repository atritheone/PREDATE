"""Ten frozen-backend stop/continue cycles on a disposable copy of a saved hunt."""
from pathlib import Path
import argparse
import json
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import Bridge, fixture, native_evidence
from engine_lifecycle import native_command
from engine_pause import wait_client, wait_server
import platform_win
import session
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='op-261117-224448-669a')
    parser.add_argument('--cycles', type=int, default=10)
    args = parser.parse_args()
    if not 2 <= args.cycles <= 20: parser.error('--cycles must be between 2 and 20')
    assert not any(platform_win.find_processes(name) for name in
                   ('DayZ_x64.exe','DayZServer_x64.exe','DayZDiag_x64.exe'))
    assert not session.manager_alive(), 'This proof requires a fresh matching frozen supervisor'
    preferences = store.preferences()
    source_files = {name:store.digest(path) for path,name in store.save_files(args.source)}
    profile = fixture(args.source, session_equipment=True)
    bridge = Bridge()
    report = dict(profile=profile, source=args.source, passed=False, cycles=[],
                  packaged_backend=True, test_ai_simulation_held=True,
                  backend_sha256=store.digest(ROOT/'launcher-app/backend/PREDATEBackend.exe'))
    report.update(native_evidence())
    saved = None
    progress = None
    previous_player = None
    try:
        # Seed one initial session, then perform the requested number of actual
        # continuations of its saved state. Cycle zero is preparation.
        for cycle in range(args.cycles+1):
            bridge.wait_action(bridge.action('continue', profile=profile))
            owner = session.read_native(session.MANAGER)
            assert store.digest(Path(owner['executable'])) == report['backend_sha256']
            ready = world.native_status()
            if saved:
                assert abs(ready['SimulationSeconds']-saved['SimulationSeconds']) < .02, ready
                assert abs(ready['PulseRemaining']-progress['PulseRemaining']) < .02, ready
                assert ready['BotMagazine'] == 17, ready
                for native, field in [('BotHealth','BotHealth'),('BotBlood','BotBlood'),('BotShock','BotShock')]:
                    assert abs(ready[native]-saved[field]) < .1, (native,ready[native],saved[field])
            bridge.wait_phase({'playing'})
            active = wait_server(lambda value:value.get('PlayerMagazine') == 13 and value.get('BotMagazine') == 17, 20)
            assert 0 < active['PlayerHealth'] <= 100 and 0 < active['BotHealth'] <= 100
            if previous_player:
                # A few active frames may legitimately regenerate vitals after
                # connection. A new full-health/blood body is not continuation.
                assert abs(active['PlayerHealth']-previous_player['PlayerHealth']) < 3, active
                assert abs(active['PlayerBlood']-previous_player['PlayerBlood']) < 100, active
            item = {'cycle':cycle, 'restored_attached_ammo':[active['PlayerMagazine'],active['BotMagazine']]}
            if cycle > 0 and cycle % 3 == 0:
                for repeat in range(2):
                    native_command('pause')
                    bridge.wait_phase({'paused'},15)
                    paused = wait_server(lambda value:value.get('Paused'))
                    time.sleep(.3)
                    assert world.native_status() == paused
                    native_command('request-resume')
                    bridge.wait_phase({'playing'},30)
                    wait_client(lambda value:not value.get('PauseMenu') and value.get('Phase') == 'connected')
                native_command('pause')
                bridge.wait_phase({'paused'},15)
                item['two_resumes_and_paused_save'] = True
            bridge.wait_action(bridge.action('quit'),90)
            bridge.wait_phase({'suspended'})
            assert not world.running() and not platform_win.find_processes('DayZ_x64.exe')
            last = store.read_json(world.WORLD/'last-session.json')
            assert last['clean'] and last['backup'], last
            store.validate_snapshot(ROOT/'backups'/last['backup'])
            saved = store.read_json(world.PROFILE/'PredateWorldState.json')
            progress = store.read_json(world.PROFILE/'PredateProgress.json')
            previous_player = world.native_status()
            closed_files = {name:store.digest(path) for path,name in store.save_files(profile)}
            time.sleep(.5)
            assert closed_files == {name:store.digest(path) for path,name in store.save_files(profile)}
            item.update(clean_snapshot=last['backup'], simulation_seconds=saved['SimulationSeconds'],
                        pulse_remaining=progress['PulseRemaining'])
            report['cycles'].append(item)
            store.atomic_json(ROOT/'build/engine-session-stress-results.json',report)
            label = f'clean stop/continue cycle {cycle}/{args.cycles}' if cycle else 'initial injured/ammunition fixture saved'
            print('PASS: '+label,flush=True)
        report['completed_continue_cycles'] = args.cycles
        report['passed'] = True
    finally:
        try:
            if platform_win.find_processes('DayZ_x64.exe') or platform_win.find_processes('DayZServer_x64.exe'):
                bridge.wait_action(bridge.action('quit'),90)
        finally:
            bridge.close()
            store.save_preferences({key:value for key,value in preferences.items() if key != 'schema'})
            report['user_preferences_unchanged'] = preferences == store.preferences()
            report['source_save_unchanged'] = source_files == {name:store.digest(path) for path,name in store.save_files(args.source)}
            store.atomic_json(ROOT/'build/engine-session-stress-results.json',report)
            store.atomic_json(ROOT/f'build/engine-session-stress-{profile}.json',report)
            print(json.dumps(report,indent=2),flush=True)
            assert report['user_preferences_unchanged'] and report['source_save_unchanged']


if __name__ == '__main__': main()
