"""Exercise native dispersed operators and both sides of the insertion rule."""
import argparse
import math
import os
from pathlib import Path
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_loadouts import close_client
from engine_sessions import native_evidence
import build
import session
import session_store as store
import spawn_points
import world


PROBE = '''
class PredateMapInsertionReport
{
    vector Player;
    vector Raven;
    vector Lynx;
    ref array<vector> Others = new array<vector>;
}

class PredateDensityActivity
{
    int ActivePeers;
    int MovedPeers;
    int PeersWithPulses;
    int Frames;
    float Elapsed;
    float WorstFrameSeconds;
}

class PredateMapInsertionMission : MissionServer
{
    bool m_Killed;
    bool m_Reported;
    bool m_MeasuredDensity;
    float m_DensityElapsed;
    float m_WorstDensityFrame;
    int m_DensityFrames;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (!m_PredateSession.m_ActorsActive || !m_PredateSession.m_PeerRoster) return;
        m_DensityElapsed += timeslice;
        m_DensityFrames++;
        m_WorstDensityFrame = Math.Max(m_WorstDensityFrame, timeslice);
        if (!m_MeasuredDensity && m_DensityElapsed >= 90 && m_PredateSession.m_HuntConfig.OpponentCount > 6)
        {
            PredateDensityActivity activity = new PredateDensityActivity;
            activity.Frames = m_DensityFrames;
            activity.Elapsed = m_DensityElapsed;
            activity.WorstFrameSeconds = m_WorstDensityFrame;
            foreach (PredatePeerBinding peer : m_PredateSession.m_PeerRoster.m_Bindings)
            {
                if (!peer.Actor || !peer.Actor.IsAlive()) continue;
                activity.ActivePeers++;
                if (PredateHunterBrain.Distance(peer.Actor.GetPosition(), peer.State.Start) >= 5) activity.MovedPeers++;
                if (peer.State.Receiver.NextSampleSerial > 1) activity.PeersWithPulses++;
            }
            string measurementError;
            JsonFileLoader<PredateDensityActivity>.SaveFile("$profile:DensityActivity.json", activity, measurementError);
            m_MeasuredDensity = true;
        }
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.IsAlive() || !player.GetIdentity()) continue;
            PredatePeerBinding lynx = m_PredateSession.m_PeerRoster.Find("lynx-01");
            eAIBase raven = m_PredateSession.m_HuntBot;
            if (!m_Killed && raven && lynx.Actor && FileExist("$profile:EndRivalLives.txt"))
            {
                DeleteFile("$profile:EndRivalLives.txt");
                // Force both replacement planners to reject their first candidate.
                m_PredateSession.m_HuntConfig.BotStart = player.GetPosition();
                lynx.State.Start = player.GetPosition();
                raven.SetHealth("", "Health", 0);
                lynx.Actor.SetHealth("", "Health", 0);
                m_Killed = true;
            }
            bool replaced = m_PredateSession.m_HuntProgress.Operator.Deployment == 2 && lynx.State.Operator.Deployment == 2;
            if (m_Killed && !m_Reported && raven && lynx.Actor && raven.IsAlive() && lynx.Actor.IsAlive() && replaced)
            {
                PredateMapInsertionReport result = new PredateMapInsertionReport;
                result.Player = player.GetPosition();
                result.Raven = raven.GetPosition();
                result.Lynx = lynx.Actor.GetPosition();
                foreach (PredatePeerBinding binding : m_PredateSession.m_PeerRoster.m_Bindings)
                    if (binding != lynx && binding.Actor && binding.Actor.IsAlive()) result.Others.Insert(binding.Actor.GetPosition());
                string error;
                JsonFileLoader<PredateMapInsertionReport>.SaveFile("$profile:ReplacementPositions.json", result, error);
                m_Reported = true;
            }
            if (FileExist("$profile:EndPlayerLife.txt"))
            {
                DeleteFile("$profile:EndPlayerLife.txt");
                player.SetHealth("", "Health", 0);
            }
            return;
        }
    }
}
'''


def horizontal(a, b):
    return math.hypot(a[0] - b[0], a[2] - b[2])


def wait_for(read, condition, timeout=40):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        assert world.running(), 'Owned server stopped before the spawn proof completed'
        failure = session.read_native(world.WORLD/'client/PredateFailure.json')
        assert not failure, failure
        value = read()
        if condition(value):
            return value
        time.sleep(.15)
    raise TimeoutError('Native map-spawn fixture did not reach the expected state')


def check_presets():
    profile = 'validation-presets-' + uuid.uuid4().hex[:6]
    info = store.create_operation('Native spawn preset validation', profile=profile, extra_mods=[])
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json', info)
    mission = world.MISSION/'init.c'
    factory = 'Mission CreateCustomMission(string path)'
    entries = '\n'.join(f'        Check("{item["id"]}", "{item["x"]} 0 {item["z"]}", report);'
                        for item in spawn_points.options())
    probe = '''
class PredatePresetReport
{
    ref array<string> Usable = new array<string>;
    ref array<string> FitsThirty = new array<string>;
    ref array<string> IrregularThirty = new array<string>;
}

class PredatePresetMission : MissionServer
{
    bool m_Checked;
    void Check(string id, vector point, PredatePresetReport report)
    {
        if (!PredateSpawnPlanner.PlayerTerrain(point)) return;
        report.Usable.Insert(id);
        array<vector> occupied = {point};
        vector raven;
        if (!PredateSpawnPlanner.OperatorSite(m_PredateSession, point, occupied, PredateDeployment.MINIMUM_DISTANCE, raven)) return;
        occupied.Insert(raven);
        array<vector> peers = {};
        if (!PredateSpawnPlanner.RegionalSites(m_PredateSession, occupied, 29, peers)) return;
        for (int i = 0; i < occupied.Count(); i++)
            for (int j = i + 1; j < occupied.Count(); j++)
                if (PredateHunterBrain.Distance(occupied[i], occupied[j]) < 1500) return;
        report.FitsThirty.Insert(id);
        int varied;
        foreach (vector peer : peers)
        {
            float gridX = Math.Round((peer[0] - 850) / 1750) * 1750 + 850;
            float gridZ = Math.Round((peer[2] - 850) / 1750) * 1750 + 850;
            if (PredateHunterBrain.Distance(peer, Vector(gridX, 0, gridZ)) > 200) varied++;
        }
        if (varied >= 20) report.IrregularThirty.Insert(id);
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_Checked || !m_PredateSession.m_HuntReady) return;
        m_Checked = true;
        PredatePresetReport report = new PredatePresetReport;
''' + entries + '''
        string error;
        JsonFileLoader<PredatePresetReport>.SaveFile("$profile:UsablePresets.json", report, error);
    }
}
'''
    mission.write_text(mission.read_text().replace(factory, probe+'\n'+factory).replace(
                       'return new MissionServer;', 'return new PredatePresetMission;'))
    evidence = dict(profile=profile, **native_evidence())
    try:
        world.start()
        output = wait_for(lambda: session.read_native(world.PROFILE/'UsablePresets.json'), bool)
        assert output['Usable'] == [option['id'] for option in spawn_points.options()], output
        assert output['FitsThirty'] == output['Usable'], output
        assert output['IrregularThirty'] == output['Usable'], output
        store.atomic_json(ROOT/'build/engine-spawn-presets-results.json', dict(evidence, passed=True, **output))
        print(output, flush=True)
    finally:
        if world.running(): world.stop(make_backup=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--presets-only', action='store_true')
    parser.add_argument('--opponents', type=int, choices=(6, 15, 30), default=6)
    args = parser.parse_args()
    build.require_closed_runtime()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    if args.presets_only:
        check_presets()
        return
    profile = 'validation-map-' + uuid.uuid4().hex[:6]
    point = [4500.25, 0, 6501.75]
    info = store.create_operation('Map-wide rival validation', profile=profile, extra_mods=[], opponent_count=args.opponents,
                                  spawn={'x': point[0], 'z': point[2]})
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json', info)
    config_path = world.PROFILE/'PredateConfig.json'
    config = store.read_json(config_path)
    config['OperatorRespawnDelay'] = 5
    store.atomic_json(config_path, config)
    mission = world.MISSION/'init.c'
    factory = 'Mission CreateCustomMission(string path)'
    mission.write_text(mission.read_text().replace(factory, PROBE+'\n'+factory).replace(
        'return new MissionServer;', 'return new PredateMapInsertionMission;'))
    report = dict(profile=profile, passed=False, opponents=args.opponents, **native_evidence())
    client = world.WORLD/'client'
    roster_path = world.PROFILE/'PredatePeerRoster.json'
    nonce = ''
    try:
        world.start()
        nonce = store.read_json(world.STATE)['session_id']
        roster = store.read_json(roster_path)['Peers']
        assert len(roster) == args.opponents - 1
        assert len({peer['Operator']['StorageKey'] for peer in roster}) == args.opponents - 1
        ready = wait_for(world.native_status, lambda status: status.get('Ready') and 'BotPosition' in status)
        sites = [point, ready['BotPosition'], *(peer['Start'] for peer in roster)]
        report['initial_minimum_spacing'] = min(horizontal(a, b) for i, a in enumerate(sites) for b in sites[i+1:])
        report['rival_span_metres'] = max(horizontal(a, b) for a in sites[1:] for b in sites[1:])
        expected_span = 12000 if args.opponents > 6 else 7500
        assert report['initial_minimum_spacing'] >= 1500 and report['rival_span_metres'] >= expected_span

        client.mkdir(exist_ok=True)
        (client/'PredateDeploymentValidation.txt').touch()
        world.launch_client(nonce, extra_args=['-window','-x=1280','-y=720','-predateDeploymentValidation'])
        connected = wait_for(world.native_status, lambda status: status.get('Players') == 1, timeout=165)
        assert horizontal(connected['PlayerPosition'], point) < .1, connected
        report['initial_exact_spawn'] = True
        (world.PROFILE/'EndRivalLives.txt').touch()
        trial = wait_for(lambda:session.read_native(world.PROFILE/'ReplacementPositions.json'), bool)
        occupied = [trial['Player'], *trial['Others']]
        assert len(occupied) == args.opponents - 1
        report['replacement_minimum_spacing'] = min(horizontal(pos, other) for pos in (trial['Raven'], trial['Lynx'])
                                                    for other in occupied+[trial['Lynx'] if pos == trial['Raven'] else trial['Raven']])
        assert report['replacement_minimum_spacing'] >= 1500, trial

        (world.PROFILE/'EndPlayerLife.txt').touch()
        wait_for(world.native_status, lambda status: status.get('PlayerAwaitingRedeploy'))
        click = client/'PredateRedeployClick.txt'
        unsuitable = trial['Raven']
        click.write_text(f'{unsuitable[0]:.3f} 0 {unsuitable[2]:.3f}\n')
        denied = wait_for(world.native_status, lambda status: status.get('PlayerSpawnRejected'))
        assert denied['PlayerLife'] == 1 and denied['PlayerAwaitingRedeploy']
        report['nearby_rival_spawn_rejected'] = True
        selected = [4500.75, 0, 6501.25]
        click.write_text(f'{selected[0]} 0 {selected[2]}\n')
        deployed = wait_for(world.native_status, lambda status: status.get('PlayerLife') == 2)
        assert horizontal(deployed['PlayerPosition'], selected) < .1, deployed
        report['exact_player_redeploy'] = True
        if args.opponents > 6:
            activity = wait_for(lambda:session.read_native(world.PROFILE/'DensityActivity.json'), bool, timeout=140)
            assert activity['ActivePeers'] == args.opponents - 1, activity
            assert activity['PeersWithPulses'] == args.opponents - 1, activity
            assert activity['MovedPeers'] >= args.opponents * .7, activity
            activity['AverageFramesPerSecond'] = round(activity['Frames'] / activity['Elapsed'], 2)
            report['live_activity'] = activity
        close_client(nonce)
        world.stop(make_backup=False)

        saved = store.read_json(roster_path)
        assert saved['Peers'][0]['Operator']['StorageKey'] == 'PredateLynx01_d2'
        assert store.read_json(world.PROFILE/'PredateProgress.json')['Operator']['StorageKey'] == 'PredateRaven01_d2'
        world.install_mission()
        world.start()
        assert store.read_json(roster_path) == saved
        report['passed'] = True
    finally:
        close_client(nonce)
        if world.running(): world.stop(make_backup=False)
        name = 'map-spawns' if args.opponents == 6 else f'map-spawns-{args.opponents}'
        store.atomic_json(ROOT/f'build/engine-{name}-results.json', report)
        print(report, flush=True)


if __name__ == '__main__': main()
