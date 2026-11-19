"""Cold-start a new active hunt without opening or selecting a user's operation."""
import argparse
from pathlib import Path
import json
import os
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import native_evidence
import platform_win
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--opponents', type=int, choices=(1, 3), default=1)
    args = parser.parse_args()
    assert not any(platform_win.find_processes(n) for n in
                   ('DayZ_x64.exe','DayZServer_x64.exe','DayZDiag_x64.exe'))
    preferences = store.preferences()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    info = store.create_operation('New-hunt validation',
                                  profile='validation-newhunt-'+uuid.uuid4().hex[:6], extra_mods=[],
                                  opponent_count=args.opponents)
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json', info)
    report = dict(profile=info['id'], candidate=True, passed=False)
    report.update(native_evidence())
    try:
        world.start()
        config = store.read_json(world.PROFILE/'PredateConfig.json')
        assert all(config[key] for key in ('EnableFairInformation','EnableSharedSignals','EnableStrategicHunter'))
        assert config['OpponentCount'] == args.opponents, config
        status = world.native_status()
        assert status['Ready'] and not status['Defeated'] and status['Players'] == 0
        assert status['BotHealth'] == 100 and status['BotSimulationDisabled']
        state = store.read_json(world.PROFILE/'PredateWorldState.json')
        assert all(state[key] == 1 for key in ('InformationSchema','SignalSchema','HunterSchema'))
        assert status['SimulationSeconds'] == 0
        roster = world.PROFILE/'PredatePeerRoster.json'
        if args.opponents == 3:
            peers = store.read_json(roster)['Peers']
            assert [(peer['Operator']['Id'], peer['Operator']['LifeState']) for peer in peers] == [('lynx-01', 'alive'), ('rook-01', 'alive')]
            report['additional_opponents_ready'] = True
        else:
            assert not roster.exists()
        report.update(active_hunter_defaults=True, initialized_without_a_player=True,
                      no_gameplay_progress_before_connection=True, passed=True)
    finally:
        if world.running(): world.stop(make_backup=False)
        report['user_preferences_unchanged'] = preferences == store.preferences()
        assert report['user_preferences_unchanged']
        name = 'new-hunt-roster' if args.opponents == 3 else 'new-hunt'
        store.atomic_json(ROOT/f'build/engine-{name}-results.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
