"""Validate hunter activation on a copy of an existing operation, without a client."""
from pathlib import Path
import argparse
import json
import os
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import fixture, native_evidence
import platform_win
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='op-261117-224448-669a')
    args = parser.parse_args()
    assert not any(platform_win.find_processes(n) for n in
                   ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe'))
    before = {name: store.digest(path) for path, name in store.save_files(args.source)}
    source = store.world_path(args.source)/'profile'
    saved = store.read_json(source/'PredateWorldState.json')
    progress = store.read_json(source/'PredateProgress.json')
    previous_status = store.read_json(source/'PredateSessionStatus.json', {})
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = fixture(args.source)
    # Keep the production mission: this is an upgrade proof, not a seeded range.
    world.install_mission()
    path = world.PROFILE/'PredateConfig.json'
    config = store.read_json(path)
    config.update(EnableFairInformation=True, EnableSharedSignals=True, EnableStrategicHunter=True)
    store.atomic_json(path, config)
    report = dict(profile=profile, source=args.source, passed=False, candidate=True)
    report.update(native_evidence())
    try:
        world.start()
        status = world.native_status()
        assert status['Ready'] and status['Players'] == 0
        assert status['BotSimulationDisabled']
        assert abs(status['SimulationSeconds']-saved['SimulationSeconds']) < .02
        assert abs(status['PulseRemaining']-progress['PulseRemaining']) < .02
        for native, persisted in (('BotHealth','BotHealth'), ('BotBlood','BotBlood'), ('BotShock','BotShock')):
            if saved.get('HasBotVitals'): assert abs(status[native]-saved[persisted]) < .1
        for key in ('BotMagazine', 'BotPosition'):
            if key in previous_status: assert status[key] == previous_status[key], (key, status[key], previous_status[key])
        migrated = store.read_json(world.PROFILE/'PredateWorldState.json')
        assert all(migrated.get(key) == 1 for key in ('InformationSchema','SignalSchema','HunterSchema'))
        report.update(ready_without_a_player=True, preserved_clock_and_pulse=True,
                      preserved_operator_vitals_and_inventory=True, upgraded_private_schemas=True)
        report['passed'] = True
    finally:
        if world.running(): world.stop(make_backup=False)
        report['source_save_unchanged'] = before == {name:store.digest(path) for path,name in store.save_files(args.source)}
        assert report['source_save_unchanged']
        store.atomic_json(ROOT/'build/engine-upgrade-results.json', report)
        store.atomic_json(ROOT/f'build/engine-upgrade-{profile}.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
