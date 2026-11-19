"""Unattended real-client stability check in a disposable validation world.

The test character is invulnerable so combat cannot end the timer/RPC exercise.
This flag exists only in the temporary mission fixture, never in production.
"""
import argparse
import json
import os
from pathlib import Path
import sys
import signal
import subprocess
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import engine_sessions as engine
import platform_win
import session
import session_store as store
import world


NATIVE_METRICS = '''
class PredateStabilityMetrics
{
    int Frames;
    float WorstFrameSeconds;
    float WindowSeconds;
    string RavenSurface;
    string PlayerSurface;
}

class PredateStabilityMission : PredateValidationMission
{
    ref PredateStabilityMetrics m_Metrics = new PredateStabilityMetrics;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!m_PredateSession.m_ActorsActive) return;
        m_Metrics.Frames++;
        m_Metrics.WindowSeconds += timeslice;
        m_Metrics.WorstFrameSeconds = Math.Max(m_Metrics.WorstFrameSeconds, timeslice);
        if (m_Metrics.WindowSeconds < 15) return;

        if (m_PredateSession.m_HuntBot)
        {
            vector raven = m_PredateSession.m_HuntBot.GetPosition();
            GetGame().SurfaceGetType3D(raven[0], raven[1] + 0.2, raven[2], m_Metrics.RavenSurface);
        }
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man player : players)
        {
            if (!player.GetIdentity()) continue;
            vector position = player.GetPosition();
            GetGame().SurfaceGetType3D(position[0], position[1] + 0.2, position[2], m_Metrics.PlayerSurface);
            break;
        }
        string error;
        JsonFileLoader<PredateStabilityMetrics>.SaveFile("$profile:StabilityMetrics.json", m_Metrics, error);
        m_Metrics = new PredateStabilityMetrics;
    }
}
'''


def process_memory(process_ids):
    ids = ','.join(str(pid) for pid in process_ids)
    command = (f'Get-Process -Id {ids} -ErrorAction SilentlyContinue '
               '| Select-Object ProcessName,WorkingSet64,PrivateMemorySize64 | ConvertTo-Json -Compress')
    payload = subprocess.check_output(['powershell', '-NoProfile', '-Command', command], text=True)
    processes = json.loads(payload) if payload.strip() else []
    if isinstance(processes, dict):
        processes = [processes]
    return {row['ProcessName']: {'working_set_bytes': row['WorkingSet64'],
                                 'private_bytes': row['PrivateMemorySize64']}
            for row in processes}


def saved_activity():
    roster = session.read_native(world.PROFILE/'PredatePeerRoster.json')
    state = session.read_native(world.PROFILE/'PredateWorldState.json')
    tracks = state.get('Footprints', {})
    return {
        'peer_life': {peer['Operator']['Id']: peer['Operator']['LifeState'] for peer in roster.get('Peers', [])},
        'retained_footprints': len(tracks.get('Marks', [])),
        'total_footprints': max(0, tracks.get('NextSerial', 1) - 1),
    }


def current_native_status():
    required = {'Players', 'PlayerHealth', 'PulseAge', 'SimulationSeconds'}
    deadline = time.monotonic() + 2
    while time.monotonic() < deadline:
        status = world.native_status()
        if status.get('Error') or required <= status.keys():
            return status
        time.sleep(.05)
    raise RuntimeError('Native status was unavailable for two seconds')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=900)
    parser.add_argument('--opponents', type=int, choices=store.OPPONENT_COUNTS, default=1)
    parser.add_argument('--candidate', action='store_true')
    parser.add_argument('--footprints', action='store_true')
    args = parser.parse_args()
    if args.seconds < 120: raise ValueError('Use at least two minutes to exercise pulse timers')
    assert not platform_win.find_processes('DayZ_x64.exe') and not platform_win.find_processes('DayZServer_x64.exe')
    assert not session.manager_alive(), 'Use a fresh matching supervisor for validation'
    preferences = store.preferences()
    engine.FIXTURE = engine.FIXTURE.replace('player.SetHealth("", "Health", 81);',
        'player.SetAllowDamage(false);\n            player.SetHealth("", "Health", 81);')
    if args.candidate:
        os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = engine.fixture('crashlab-primitive-send-d1612')
    mission = world.MISSION/'init.c'
    factory = 'Mission CreateCustomMission(string path)'
    authored = mission.read_text(encoding='utf-8')
    mission.write_text(authored.replace(factory, NATIVE_METRICS+'\n'+factory)
                       .replace('return new PredateValidationMission;', 'return new PredateStabilityMission;'),
                       encoding='utf-8')
    if args.opponents > 1 or args.footprints:
        config_path = world.PROFILE/'PredateConfig.json'
        config = store.read_json(config_path)
        config.update(OpponentCount=args.opponents, EnableFootprints=args.footprints,
                      EnableFairInformation=True, EnableSharedSignals=True, EnableStrategicHunter=True)
        store.atomic_json(config_path, config)
    bridge = engine.Bridge(candidate=args.candidate)
    report = {'profile': profile, 'passed': False, 'requested_seconds': args.seconds,
              'test_character_invulnerable': True, 'opponents': args.opponents, 'footprints': args.footprints}
    report.update(engine.native_evidence())
    started = None
    pulses = 0
    try:
        bridge.wait_action(bridge.action('continue', profile=profile))
        bridge.wait_phase({'playing'})
        process_ids = platform_win.find_processes('DayZ_x64.exe')
        identity = {pid: world.process_identity(pid) for pid in process_ids}
        server_pid = store.read_json(world.STATE)['pid']
        server_identity = world.process_identity(server_pid)
        assert server_identity and len(identity) == 1 and all(identity.values())
        initial = current_native_status()
        last_pulse_age = initial['PulseAge']
        started = time.monotonic()
        next_notice = 0
        samples = []
        report['samples'] = samples
        bridge_reopened = False
        manager_recovered = False
        while time.monotonic() - started < args.seconds:
            assert world.running() and world.process_identity(server_pid) == server_identity, 'Owned server exited or restarted'
            assert all(world.process_identity(pid) == saved for pid, saved in identity.items()), 'Client exited/restarted'
            status = bridge.call('status')
            assert status['phase'] == 'playing', status
            native = current_native_status()
            assert not native.get('Error'), native.get('Error')
            assert native['Players'] == 1 and native['PlayerHealth'] > 0
            if native['PulseAge'] + 1 < last_pulse_age: pulses += 1
            last_pulse_age = native['PulseAge']
            elapsed = time.monotonic() - started
            if elapsed >= 60 and not bridge_reopened:
                bridge.close()
                time.sleep(2)
                assert world.running() and all(world.process_identity(pid) == saved for pid, saved in identity.items())
                bridge = engine.Bridge(candidate=args.candidate)
                bridge.wait_phase({'playing'}, 15)
                bridge_reopened = True
                report['launcher_bridge_reopened_during_play'] = True
            if elapsed >= 120 and not manager_recovered:
                manager = session.read_native(session.MANAGER)
                assert bridge.call('status')['profile'] == profile
                assert world.process_identity(manager['pid']) == manager
                os.kill(manager['pid'], signal.SIGTERM)
                deadline = time.monotonic() + 5
                while world.process_identity(manager['pid']) == manager and time.monotonic() < deadline:
                    time.sleep(.05)
                assert world.process_identity(manager['pid']) != manager
                bridge.wait_action(bridge.action('continue', profile=profile), 30)
                bridge.wait_phase({'playing'}, 15)
                assert world.running() and all(world.process_identity(pid) == saved for pid, saved in identity.items())
                manager_recovered = True
                report['supervisor_recovered_existing_processes'] = True
            if elapsed >= next_notice:
                samples.append({'wall_seconds': round(elapsed, 2),
                                'simulation_seconds': native['SimulationSeconds'],
                                'native_server': session.read_native(world.PROFILE/'StabilityMetrics.json'),
                                'process_memory': process_memory([server_pid, *process_ids]), **saved_activity()})
                store.atomic_json(world.PROFILE/'StabilityProgress.json',
                                  {'wall_seconds': round(elapsed, 2), 'pulse_cycles': pulses, **report})
                print(f'STABLE: {elapsed:.0f}s, pulse cycles={pulses}', flush=True)
                next_notice = elapsed + 30
            time.sleep(1)
        report['connected_seconds'] = round(time.monotonic() - started, 2)
        report['pulse_cycles'] = pulses
        assert pulses >= 2
        report['simulation_seconds'] = round(native['SimulationSeconds'] - initial['SimulationSeconds'], 2)
        assert report['simulation_seconds'] >= args.seconds - 3
        server_windows = [sample['native_server'] for sample in samples if sample['native_server']]
        assert server_windows and all(window['Frames'] > 0 and window['WindowSeconds'] >= 15 for window in server_windows)
        report['native_server_summary'] = {
            'observed_windows': len(server_windows),
            'minimum_sampled_average_fps': round(min(window['Frames']/window['WindowSeconds']
                                                     for window in server_windows), 2),
            'worst_sampled_frame_seconds': max(window['WorstFrameSeconds'] for window in server_windows),
            'observed_surfaces': sorted({window[key] for window in server_windows
                                         for key in ('PlayerSurface', 'RavenSurface') if window[key]}),
        }
        bridge.wait_action(bridge.action('quit'))
        bridge.wait_phase({'suspended'})
        assert not world.running() and not platform_win.find_processes('DayZ_x64.exe')
        if args.opponents > 1:
            report['peer_states'] = [
                {'id': peer['Operator']['Id'], 'life': peer['Operator']['LifeState'],
                 'attempted_pulse_slots': peer['Receiver']['NextSampleSerial'] - 1,
                 'route_revisions': peer['Hunter']['GoalRevision']}
                for peer in store.read_json(world.PROFILE/'PredatePeerRoster.json')['Peers']
            ]
            assert len(report['peer_states']) == args.opponents - 1
        if args.footprints:
            report['footprint_activity'] = saved_activity()
            assert report['footprint_activity']['total_footprints'] > 0
            assert report['footprint_activity']['retained_footprints'] <= 128
        report['clean_save_after_stability_run'] = True
        report['passed'] = True
    except Exception as error:
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        if started is not None:
            report.setdefault('connected_seconds', round(time.monotonic() - started, 2))
            report.setdefault('pulse_cycles', pulses)
        try:
            if world.running() or platform_win.find_processes('DayZ_x64.exe'):
                bridge.wait_action(bridge.action('quit'), 90)
                report['clean_save_after_stability_run'] = not world.running() and not platform_win.find_processes('DayZ_x64.exe')
        finally:
            try: bridge.close()
            finally:
                store.save_preferences({k: v for k, v in preferences.items() if k != 'schema'})
                name = 'stability-roster' if args.opponents == 3 else 'stability'
                if args.opponents > 3:
                    name += f'-{args.opponents}'
                if args.footprints:
                    name += '-footprints'
                if args.seconds >= 3600:
                    name += '-hour'
                store.atomic_json(store.ROOT/f'build/engine-{name}-results.json', report)
                print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
