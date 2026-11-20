"""Measure how quickly a moving Operator acquires players and rivals in DayZ."""

import argparse
import os
from pathlib import Path
from statistics import median
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))

import build
import platform_win
import session
import session_store as store
import world


def close_client(nonce):
    if not platform_win.find_processes('DayZ_x64.exe'):
        return
    if store.read_json(world.STATE).get('session_id') != nonce:
        raise RuntimeError('The validation client no longer owns the session')
    client_profile = world.WORLD / 'client'
    status = session.read_native(client_profile / 'PredateClientStatus.json')
    store.atomic_json(client_profile / 'PredateClientCommand.json', {
        'Schema': 1, 'SessionId': nonce, 'Sequence': status.get('Acknowledged', 0) + 1,
        'Action': 'quit' if world.running() else 'server-closed',
    })
    deadline = time.monotonic() + 30
    while platform_win.find_processes('DayZ_x64.exe') and time.monotonic() < deadline:
        time.sleep(.2)
    if platform_win.find_processes('DayZ_x64.exe'):
        raise TimeoutError('The validation client did not close')


def prepare(classic):
    build.require_closed_runtime()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = 'validation-acquisition-' + uuid.uuid4().hex[:6]
    info = store.create_operation('Operator acquisition validation', profile=profile, extra_mods=[])
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD / 'operation.json', info)
    settings_path = world.PROFILE / 'PredateConfig.json'
    settings = store.read_json(settings_path)
    settings.update(EnableFairInformation=True, EnableSharedSignals=False, EnableStrategicHunter=False,
                    EnableOperatorRespawns=False, RandomInitialSpawns=False, ThreatDistance=1000,
                    EliteAI=not classic, PlayerStart=[11700, 0, 12200], BotStart=[11730, 0, 12220])
    store.atomic_json(settings_path, settings)
    mission = world.MISSION / 'init.c'
    script = mission.read_text(encoding='utf-8')
    factory = 'Mission CreateCustomMission(string path)'
    if script.count(factory) != 1 or script.count('return new MissionServer;') != 1:
        raise RuntimeError('The mission factory no longer matches the validation fixture')
    probe = Path(__file__).with_name('elite_acquisition.c').read_text(encoding='utf-8')
    mission.write_text(script.replace(factory, probe + '\n' + factory).replace(
        'return new MissionServer;', 'return new PredateAcquisitionMission;'), encoding='utf-8')
    return profile


def collect():
    deadline = time.monotonic() + 520
    while world.running() and time.monotonic() < deadline:
        failure = session.read_native(world.WORLD / 'client/PredateFailure.json')
        if failure:
            raise RuntimeError(failure.get('message', 'The validation client failed'))
        result = session.read_native(world.PROFILE / 'EliteAcquisition.json')
        if result.get('Trials'):
            return result['Trials']
        time.sleep(1)
    raise RuntimeError('The acquisition fixture did not finish in DayZ')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--classic', action='store_true', help='Run the same exposures with Classic AI')
    args = parser.parse_args()
    profile = prepare(args.classic)
    label = 'classic' if args.classic else 'elite'
    report = {'profile': profile, 'preset': label, 'passed': False,
              'addon_sha256': store.digest(world.MODS[-1] / 'addons/Predate.pbo'),
              'mission_sha256': store.digest(world.MISSION / 'init.c'),
              'dependency_lock_sha256': store.digest(ROOT / 'config/release-lock.json')}
    nonce = ''
    try:
        world.start()
        nonce = store.read_json(world.STATE)['session_id']
        world.launch_client(nonce, extra_args=['-window', '-x=1280', '-y=720'])
        trials = collect()
        report['trials'] = trials
        if len(trials) != 24 or not all(trial['StartedWithoutTarget'] for trial in trials):
            raise RuntimeError('The 24 unseeded acquisition trials did not finish')
        eligible = [trial for trial in trials if trial['ClearOpening'] and trial['StartedMoving'] and trial['SightLimit'] >= trial['Range']]
        confirmed = [trial for trial in eligible if trial['ConfirmedSeconds'] >= 0]
        report['clear_openings'] = len(eligible)
        report['confirmed'] = len(confirmed)
        report['confirmed_while_traveling'] = sum(trial['MovementSpeedAtConfirmation'] >= 1 for trial in confirmed)
        report['median_confirmation_seconds'] = median(trial['ConfirmedSeconds'] for trial in confirmed) if confirmed else None
        if len(eligible) < 12 or not confirmed:
            raise RuntimeError('Insufficient usable DayZ exposures')
        report['passed'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        try:
            if nonce:
                close_client(nonce)
        finally:
            if world.running():
                world.stop(make_backup=False)
            path = ROOT / f'build/engine-acquisition-{label}-results.json'
            store.atomic_json(path, report)
            print(path, flush=True)


if __name__ == '__main__':
    main()
