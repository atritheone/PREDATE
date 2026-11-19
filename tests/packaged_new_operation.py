"""Create and reopen a disposable operation using the installed addon and frozen backend."""
import argparse
from pathlib import Path
import json
import math
import os
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import Bridge, native_evidence
from engine_lifecycle import native_command
from engine_pause import wait_server
from test_operation_setup import entry, settings
import platform_win
import session
import session_store as store
import spawn_points
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--opponents', type=int, choices=(1, 3, 6, 15, 30), default=1)
    parser.add_argument('--footprints', action='store_true')
    parser.add_argument('--candidate', action='store_true')
    parser.add_argument('--backend', type=Path, default=ROOT/'launcher-app/backend/PREDATEBackend.exe')
    parser.add_argument('--spawn-preset', choices=(*spawn_points.PRESETS, 'regional'))
    parser.add_argument('--custom-setup', action='store_true')
    args = parser.parse_args()
    assert not any(platform_win.find_processes(name) for name in
                   ('DayZ_x64.exe','DayZServer_x64.exe','DayZDiag_x64.exe'))
    assert not session.manager_alive(), 'Retain any existing supervisor; this proof requires the matching frozen service'
    preferences = store.preferences()
    user_profile = preferences['selected_world']
    user_save = {name:store.digest(path) for path,name in store.save_files(user_profile)}
    if args.candidate:
        os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    bridge = Bridge(candidate=args.candidate, bundled=args.backend)
    profile = None
    report = {'passed':False, 'packaged_backend':True, 'production_mission':True, 'opponents':args.opponents,
              'footprints':args.footprints,
              'backend_sha256':store.digest(args.backend)}
    try:
        assert [kit['id'] for kit in bridge.call('overview')['loadouts']] == ['m4a1','ak74']
        setup = {'spawn': {'preset': args.spawn_preset}} if args.spawn_preset else {}
        if args.custom_setup:
            setup['setup'] = settings(inventory=[
                entry('Mich2001Helmet', 'attachment', slot='Headgear'),
                entry('NVGoggles', 'attachment', parent=0, slot='NVG'),
                entry('Battery9V', 'attachment', parent=1, slot='BatteryD'),
                entry('AK74', 'hands'),
                entry('AK74_Hndgrd', 'attachment', parent=3, slot='WeaponHandguardAK'),
                entry('AK74_WoodBttstck', 'attachment', parent=3, slot='weaponButtstockAK'),
                entry('Mag_AK74_30Rnd', 'magazine', parent=3, amount=17),
                entry('AssaultBag_Green', 'attachment', slot='Back'),
                entry('BandageDressing', 'cargo', parent=7, row=1, column=2, amount=3),
            ])
        bridge.wait_action(bridge.action('new', name='Packaged new operation validation', loadout='ak74',
                                         opponents=args.opponents, footprints=args.footprints, **setup))
        profile = bridge.call('status')['profile']
        assert profile and profile != user_profile and profile.startswith('op-')
        world.select_profile(profile)
        info = store.metadata(profile)
        info['kind'] = 'validation'
        store.atomic_json(world.WORLD/'operation.json', info)
        report.update(profile=profile, selected_kit='ak74')
        report.update(native_evidence())
        owner = session.read_native(session.MANAGER)
        report['supervisor_sha256'] = store.digest(Path(owner['executable']))
        assert report['supervisor_sha256'] == report['backend_sha256']
        assert store.read_json(world.PROFILE/'PredateConfig.json')['PlayerLoadout'] == 'ak74'
        assert store.read_json(world.PROFILE/'PredateConfig.json')['OpponentCount'] == args.opponents
        config = store.read_json(world.PROFILE/'PredateConfig.json')
        assert bool(config.get('EnableFootprints', False)) == args.footprints
        roster_path = world.PROFILE/'PredatePeerRoster.json'
        bridge.wait_phase({'playing'})
        native_command('pause')
        bridge.wait_phase({'paused'}, 15)
        initial = wait_server(lambda value:value.get('Paused') and value.get('PlayerFirearmClass') == 'AK74')
        assert initial['PlayerHealth'] > 0, initial
        if args.custom_setup:
            assert initial['PlayerMagazine'] == 17, initial
            assert config['HasCustomInventory']
            assert not config['EnableOperatorRespawns'] and config['OperatorRespawnDelay'] == 145
        else:
            assert initial['PlayerMagazine'] >= 29, initial
        if args.spawn_preset:
            if args.spawn_preset == 'regional':
                x, _, z = config['PlayerStart']
                named_points = {(preset[1], preset[2]) for preset in spawn_points.PRESETS.values()}
                assert (x, z) in named_points
            else:
                _, x, z = spawn_points.PRESETS[args.spawn_preset]
            position = initial['PlayerPosition']
            assert math.hypot(position[0]-x, position[2]-z) < .1, position
            report['spawn_preset'] = args.spawn_preset
            report['resolved_spawn'] = [x, z]
            report['exact_preset_spawn'] = True
        report['actual_selected_firearm'] = initial['PlayerFirearmClass']
        report['initial_attached_ammo'] = initial['PlayerMagazine']
        bridge.wait_action(bridge.action('quit'), 90)
        bridge.wait_phase({'suspended'})
        saved = store.read_json(world.PROFILE/'PredateWorldState.json')
        if args.custom_setup:
            assert saved['Hour'] == 20 and 35 <= saved['Minute'] <= 37, saved
            report['custom_inventory_and_initial_clock'] = True
        roster = store.read_json(roster_path)
        if args.opponents > 1:
            ids = ['lynx-01', 'rook-01', 'viper-01', 'kestrel-01', 'jackal-01']
            roster_ids = [peer['Operator']['Id'] for peer in roster['Peers']]
            assert len(set(roster_ids)) == args.opponents - 1
            assert roster_ids[:len(ids)] == ids[:args.opponents-1]
            if args.opponents == 30: assert roster_ids[-1] == 'weasel-01'
        else:
            assert roster is None
        saved_files = {name:store.digest(path) for path,name in store.save_files(profile)}
        time.sleep(1)
        assert saved_files == {name:store.digest(path) for path,name in store.save_files(profile)}
        bridge.wait_action(bridge.action('continue', profile=profile))
        ready = world.native_status()
        assert abs(ready['SimulationSeconds']-saved['SimulationSeconds']) < .02, ready
        assert store.read_json(roster_path) == roster
        if args.footprints:
            reopened = store.read_json(world.PROFILE/'PredateWorldState.json')
            assert reopened['Footprints'] == saved['Footprints']
            report['footprint_ledger_preserved'] = True
        bridge.wait_phase({'playing'})
        native_command('pause')
        bridge.wait_phase({'paused'}, 15)
        reopened = wait_server(lambda value:value.get('Paused') and value.get('PlayerFirearmClass') == 'AK74')
        assert reopened['PlayerMagazine'] == initial['PlayerMagazine'], reopened
        bridge.wait_action(bridge.action('quit'), 90)
        bridge.wait_phase({'suspended'})
        report.update(passed=True, new_action_created_separate_operation=True,
                      paused_save_quit=True, selected_kit_and_ammo_preserved=True,
                      no_progress_while_closed=True, roster_round_trip=args.opponents > 1)
    finally:
        try:
            if platform_win.find_processes('DayZ_x64.exe') or platform_win.find_processes('DayZServer_x64.exe'):
                bridge.wait_action(bridge.action('quit'), 90)
        finally:
            bridge.close()
            store.save_preferences({key:value for key,value in preferences.items() if key != 'schema'})
            report['user_preferences_unchanged'] = store.preferences() == preferences
            report['user_save_unchanged'] = user_save == {name:store.digest(path) for path,name in store.save_files(user_profile)}
            name = 'packaged-new-operation'
            if args.custom_setup:
                name += '-custom-setup'
            elif args.opponents > 6:
                name += f'-{args.opponents}'
            elif args.opponents == 6:
                name += '-six'
            elif args.footprints:
                name += '-footprints'
            elif args.opponents == 3:
                name += '-roster'
            store.atomic_json(ROOT/f'build/{name}-results.json', report)
            print(json.dumps(report, indent=2), flush=True)
            assert report['user_save_unchanged'] and report['user_preferences_unchanged']


if __name__ == '__main__': main()
