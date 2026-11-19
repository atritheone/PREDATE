"""Validate independent operator identity, restoration, and active navigation."""
import argparse
import json
import os
from pathlib import Path
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import native_evidence
from engine_loadouts import close_client
import build
import session
import session_store as store
import world


def boot_and_read(filename, expected_clock=0):
    world.start()
    try:
        path = world.PROFILE / filename
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline:
            assert world.running(), 'Roster-validation server stopped'
            result = session.read_native(path)
            if result.get('Checks'):
                assert result['Failures'] == 0 and not result['Error'], result
                status = world.native_status()
                assert status['SimulationSeconds'] == expected_clock and status['BotSimulationDisabled'], status
                return result
            time.sleep(.1)
        raise TimeoutError(f'Peer report never arrived: {filename}')
    finally:
        if world.running():
            world.stop(make_backup=False)


def client_trial(report, mode):
    nonce = ''
    try:
        world.start()
        nonce = store.read_json(world.STATE)['session_id']
        extra_args = ['-window', '-x=1280', '-y=720']
        report_key = 'active_roster'
        expected = 18
        path = world.PROFILE / 'PeerActive.json'
        if mode == 'receiver':
            profile = world.WORLD / 'client'
            profile.mkdir(exist_ok=True)
            (profile / 'PredateRosterReceiverValidation.txt').touch()
            extra_args.append('-predateRosterReceiverValidation')
            path = profile / 'RosterReceiverValidation.json'
            report_key = 'roster_receiver'
            expected = 15
        elif mode == 'retarget':
            path = world.PROFILE / 'PeerRetarget.json'
            report_key = 'retarget'
            expected = 10
        elif mode == 'encounter':
            path = world.PROFILE / 'PeerEncounter.json'
            report_key = 'encounter'
            expected = 6
        world.launch_client(nonce, extra_args=extra_args)
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            assert world.running(), 'Active roster-validation server stopped'
            failure = session.read_native(world.WORLD/'client/PredateFailure.json')
            assert not failure, failure.get('message')
            result = session.read_native(path)
            if result.get('Checks'):
                report[report_key] = result
                assert result['Checks'] == expected and result['Failures'] == 0 and not result.get('Error'), result
                return result
            time.sleep(.15)
        raise TimeoutError('Active roster report never arrived')
    finally:
        if nonce:
            close_client(nonce)
        if world.running():
            world.stop(make_backup=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--active', action='store_true', help='Run a native connected-client navigation and pause trial')
    modes.add_argument('--receiver', action='store_true', help='Use the native controls to select both rival signatures')
    modes.add_argument('--retarget', action='store_true', help='Observe a surviving rival choose a new signature')
    modes.add_argument('--encounter', action='store_true', help='Test native rival acquisition without injecting target information')
    parser.add_argument('--receiver-opponents', type=int, choices=(3, 30), default=3)
    args = parser.parse_args()
    build.require_closed_runtime()
    prefs = store.preferences()
    playable = ROOT / 'mods/@Predate/addons/Predate.pbo'
    installed_hash = store.digest(playable)
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = 'validation-roster-' + uuid.uuid4().hex[:6]
    info = store.create_operation('Three-operator roster validation', profile=profile, extra_mods=[])
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD / 'operation.json', info)
    config_path = world.PROFILE/'PredateConfig.json'
    config = store.read_json(config_path)
    config['EnableOperatorRespawns'] = False
    store.atomic_json(config_path, config)
    entry = world.MISSION / 'init.c'
    factory = 'Mission CreateCustomMission(string path)'
    native = entry.read_text(encoding='utf-8')
    probe = (ROOT / 'source/validation/peer_roster_probe.c').read_text(encoding='utf-8')
    mission = 'PredatePeerActiveMission' if args.active else 'PredatePeerValidationMission'
    if args.retarget:
        mission = 'PredatePeerRetargetMission'
    if args.encounter:
        mission = 'PredatePeerEncounterMission'
    if not args.receiver:
        entry.write_text(native.replace(factory, probe + '\n' + factory).replace('return new MissionServer;', f'return new {mission};'), encoding='utf-8')
    report = dict(profile=profile, passed=False, **native_evidence())
    try:
        if args.active or args.receiver or args.retarget or args.encounter:
            config_path = world.PROFILE/'PredateConfig.json'
            config = store.read_json(config_path)
            config['ThreatDistance'] = 180
            if args.receiver or args.retarget:
                config.update(OpponentCount=3, LynxStart=[4850, 0, 2200], RookStart=[5139, 0, 2122])
            if args.receiver:
                config['OpponentCount'] = args.receiver_opponents
                report['receiver_opponents'] = args.receiver_opponents
            if args.retarget:
                config['PulseInterval'] = 10
            if args.encounter:
                config.update(OpponentCount=3, PlayerStart=[5050, 0, 2350],
                              LynxStart=[4850, 0, 2200], RookStart=[4908, 0, 2184])
            store.atomic_json(config_path, config)
            mode = 'active'
            if args.receiver:
                mode = 'receiver'
            elif args.retarget:
                mode = 'retarget'
            elif args.encounter:
                mode = 'encounter'
            checkpoint = client_trial(report, mode)
            if args.encounter:
                report['passed'] = True
                return
            if args.retarget:
                peers = store.read_json(world.PROFILE/'PredatePeerRoster.json')['Peers']
                assert peers[0]['Operator']['LifeState'] == 'defeated'
                assert peers[1]['Receiver']['SubjectId'] == 'player-01'
                assert [item['SubjectId'] for item in peers[1]['Receiver']['History']] == ['player-01']
                report['retarget_persisted'] = True
                report['passed'] = True
                return
            if args.receiver:
                peers = store.read_json(world.PROFILE/'PredatePeerRoster.json')['Peers']
                saved_world = store.read_json(world.PROFILE/'PredateWorldState.json')
                raven = saved_world['PlayerSignal']
                raven_record = store.read_json(world.PROFILE/'PredateProgress.json')['Operator']
                records = [raven_record, *(peer['Operator'] for peer in peers)]
                assert all(record['Supplies']['NextAt'] > saved_world['SimulationSeconds'] for record in records)
                report['opponent_supply_timers'] = len(records)
                rifle_kits = {record['RifleKit'] for record in records}
                if args.receiver_opponents == 30:
                    assert rifle_kits == {'m4a1', 'ak74', 'akm', 'fal', 'm14', 'sv98'}, rifle_kits
                assert all(record['EquipmentRevision'] == 1 for record in records)
                report['opponent_rifle_kits'] = sorted(rifle_kits)
                tracks = {raven['SubjectId']: raven}
                tracks.update((peer['Operator']['Id'], peer['PlayerReceiver']) for peer in peers)
                first, second = (tracks[checkpoint[name]] for name in ('First', 'Second'))
                assert first['Selected'] and not second['Selected'], (first, second)
                for track in (first, second):
                    assert abs(track['NextPulseAt'] - track['BroadcastTime'] - 90) < .005
                    assert track['BroadcastTime'] == track['LastSample']['ObservedAt']
                    assert track['LastSample']['MapRadius'] == 0
                    assert track['LastSample']['Bearing'] % 15 == 0
                    assert len(track['History']) == 1
                report['private_receiver_tracks_saved'] = True
                report['passed'] = True
                return
            reopened = boot_and_read('PeerActiveReopened.json', checkpoint['SimulationSeconds'])
            report['restored_active_roster'] = reopened
            assert reopened['SimulationSeconds'] == checkpoint['SimulationSeconds'], reopened
            assert reopened['Actors'] == checkpoint['Actors'], reopened
            lynx, rook = checkpoint['Actors']
            assert abs(rook['NextPulse'] - lynx['NextPulse'] - 3) < .005, checkpoint
            assert [lynx['FirearmClass'], rook['FirearmClass']] == ['M4A1', 'AK74'], checkpoint
            report['passed'] = True
            return
        initial = boot_and_read('PeerFirst.json')
        report['spawned'] = initial
        assert [(row['Health'], row['Blood'], row['MagazineRounds']) for row in initial['Actors']] == [(73, 4321, 17), (84, 4543, 23)], initial
        assert [row['FirearmClass'] for row in initial['Actors']] == ['M4A1', 'AK74'], initial
        roster_path = world.PROFILE/'PredatePeerRoster.json'
        legacy = store.read_json(roster_path)
        legacy['Schema'] = 1
        for peer in legacy['Peers']:
            del peer['PlayerReceiver']
        store.atomic_json(roster_path, legacy)
        restored = boot_and_read('PeerBeforeDefeat.json')
        report['restored_before_defeat'] = restored
        assert restored['Actors'] == initial['Actors'], restored
        migrated = store.read_json(roster_path)
        assert migrated['Schema'] == 2
        assert all(peer['PlayerReceiver']['OwnerId'] == 'player-01' and not peer['PlayerReceiver']['Selected']
                   for peer in migrated['Peers']), migrated
        report['legacy_roster_migrated'] = True
        finished = boot_and_read('PeerAfterDefeat.json')
        report['restored_after_defeat'] = finished
        assert finished['Actors'][0]['Life'] == 'defeated'
        assert finished['Actors'][1] == initial['Actors'][1]
        report['passed'] = True
    finally:
        report['playable_addon_unchanged'] = store.digest(playable) == installed_hash
        report['preferences_unchanged'] = store.preferences() == prefs
        name = 'active' if args.active else 'roster'
        if args.receiver:
            name = 'receiver'
        if args.retarget:
            name = 'retarget'
        if args.encounter:
            name = 'encounter'
        store.atomic_json(ROOT / f'build/engine-peer-{name}-results.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
