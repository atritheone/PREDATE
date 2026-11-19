"""Exercise the native Redeploy button before and after a clean restart."""
import argparse
import json
import os
from pathlib import Path
import re
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
import world


PROBE = '''
class PredateDeploymentValidationMission : MissionServer
{
    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_PredateSession.m_HuntReady && !m_PredateSession.m_PeerRoster && FileExist("$profile:DeploymentPeerTrial.txt"))
        {
            string error;
            m_PredateSession.AttachPeerRoster("4850 0 2200", "5139 0 2122", error);
        }
        if (!m_PredateSession.m_ActorsActive || !FileExist("$profile:EndValidationLife.txt")) return;
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.IsAlive() || !player.GetIdentity()) continue;
            DeleteFile("$profile:EndValidationLife.txt");
            eAIBase bot = m_PredateSession.m_HuntBot;
            if (bot && FileExist("$profile:DefeatValidationRaven.txt"))
            {
                DeleteFile("$profile:DefeatValidationRaven.txt");
                bot.SetHealth("", "Health", 0);
            }
            else if (bot && m_PredateSession.m_WorldState.PlayerLife == 1)
            {
                bot.SetHealth("", "Health", 73);
                bot.SetHealth("", "Blood", 4321);
                Weapon_Base rifle = Weapon_Base.Cast(bot.GetHumanInventory().GetEntityInHands());
                if (rifle)
                {
                    Magazine magazine = rifle.GetMagazine(rifle.GetCurrentMuzzle());
                    if (magazine) magazine.ServerSetAmmoCount(17);
                }
                PredateSignalService.Select(m_PredateSession, true);
                PredateSignalService.Tick(m_PredateSession, player, 0);
                PredateWorldState state = m_PredateSession.m_WorldState;
                PredateObservation remembered = new PredateObservation;
                remembered.EventKey = "validation-previous-life";
                remembered.ObserverId = "raven-01";
                remembered.SubjectId = player.GetIdentity().GetId();
                remembered.Source = PredateObservationSource.VISUAL;
                remembered.ObservedAt = state.SimulationSeconds;
                remembered.ExpiresAt = state.SimulationSeconds + 120;
                remembered.Confidence = 0.95;
                remembered.HasPosition = true;
                remembered.Estimate = player.GetPosition();
                remembered.Radius = 2;
                state.Observations.Add(remembered, state.SimulationSeconds);
                PredateHunterBrain.Goal(state.Hunter, bot.GetPosition(), remembered.Estimate, state.SimulationSeconds, "investigate", "Validation: previous visual contact", remembered.EventKey);
            }
            else if (bot && m_PredateSession.m_WorldState.PlayerLife == 2)
            {
                // Exercise alternate insertion without moving the operator during redeploy.
                bot.SetPosition(m_PredateSession.PredateGround("4520 0 6550"));
                if (m_PredateSession.m_PeerRoster)
                {
                    PredatePeerBinding nearInsertion = m_PredateSession.m_PeerRoster.Find("lynx-01");
                    nearInsertion.Actor.SetPosition(m_PredateSession.PredateGround("4500 0 6900"));
                }
                PredateWorldState destinationState = m_PredateSession.m_WorldState;
                PredateObservation sound = new PredateObservation;
                sound.EventKey = "validation-independent-sound";
                sound.ObserverId = "raven-01";
                sound.Source = PredateObservationSource.IMPACT;
                sound.ObservedAt = destinationState.SimulationSeconds;
                sound.ExpiresAt = destinationState.SimulationSeconds + 60;
                sound.Confidence = 0.6;
                sound.HasPosition = true;
                sound.Estimate = bot.GetPosition() + "40 0 0";
                sound.Radius = 10;
                destinationState.Observations.Add(sound, destinationState.SimulationSeconds);
                PredateHunterBrain.Goal(destinationState.Hunter, bot.GetPosition(), sound.Estimate, destinationState.SimulationSeconds, "investigate", "Validation: independent sound", sound.EventKey);
                destinationState.Hunter.SearchUntil = sound.ExpiresAt;
            }
            player.SetHealth("", "Health", 0);
            return;
        }
    }
}
'''


def wait_status(check, label, seconds=120):
    deadline = time.monotonic() + seconds
    status = {}
    while time.monotonic() < deadline:
        if not world.running():
            raise RuntimeError('Deployment validation server exited')
        fail = session.read_native(world.WORLD / 'client/PredateFailure.json')
        if fail:
            raise RuntimeError(fail.get('message', 'Deployment validation client failed'))
        client = session.read_native(world.WORLD / 'client/PredateClientStatus.json')
        if client.get('Error'):
            raise RuntimeError(client['Error'])
        status = world.native_status()
        if check(status):
            return status
        time.sleep(.1)
    raise TimeoutError(f'{label}: {status}')


def start_client():
    world.start()
    nonce = store.read_json(world.STATE)['session_id']
    (world.WORLD / 'client/PredateClientStatus.json').unlink(missing_ok=True)
    world.launch_client(nonce, extra_args=['-window', '-x=1920', '-y=1080', '-predateDeploymentValidation'])
    return nonce


def death(life, defeat_operator=False):
    if defeat_operator:
        (world.PROFILE / 'DefeatValidationRaven.txt').touch()
    (world.PROFILE / 'EndValidationLife.txt').touch()
    status = wait_status(lambda s: s.get('PlayerLife') == life and s.get('PlayerAwaitingRedeploy'), f'life {life} did not end', 20)
    assert status['PlayerHealth'] == 0, status
    return status


def client_state():
    return session.read_native(world.WORLD / 'client/PredateClientStatus.json')


def redeploy(life):
    (world.WORLD / 'client/PredateRedeployClick.txt').touch()
    status = wait_status(
        lambda s: s.get('PlayerLife') == life and not s.get('PlayerAwaitingRedeploy') and s.get('PlayerMagazine') == 30,
        f'Redeploy did not start life {life}', 80,
    )
    assert status['PlayerHealth'] == 100 and status['PlayerBlood'] == 5000, status
    assert status['PlayerFirearmClass'] == 'AK74' and not status['Selected'], status
    assert not (world.WORLD / 'client/PredateRedeployClick.txt').exists()
    wait_status(lambda _: client_state().get('PauseMenu') == 0 and client_state().get('Phase') == 'connected', 'deployment menu did not close', 12)
    return status


def same_raven(previous, after):
    assert previous['BotHealth'] == after['BotHealth'] == 73, (previous, after)
    assert abs(previous['BotBlood'] - after['BotBlood']) < 1, (previous, after)
    assert previous['BotMagazine'] == after['BotMagazine'] == 17, (previous, after)
    distance_squared = sum((a - b)**2 for a, b in zip(previous['BotPosition'], after['BotPosition']))
    assert distance_squared < 25, (previous, after)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--peer-roster', action='store_true', help='Check redeployment with independent operators present')
    args = parser.parse_args()
    build.require_closed_runtime()
    original_preferences = store.preferences()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = 'validation-deploy-' + uuid.uuid4().hex[:6]
    info = store.create_operation('Deployment validation', profile=profile, extra_mods=[], player_loadout='ak74')
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD / 'operation.json', info)
    config_file = world.PROFILE / 'PredateConfig.json'
    config = store.read_json(config_file)
    config.update(PlayerStart=[4500, 0, 6500], BotStart=[5500, 0, 6500], ThreatDistance=150)
    store.atomic_json(config_file, config)
    if args.peer_roster:
        (world.PROFILE / 'DeploymentPeerTrial.txt').touch()
    roster_file = world.PROFILE / 'PredatePeerRoster.json'
    entry = world.MISSION / 'init.c'
    mission = entry.read_text()
    factory = 'Mission CreateCustomMission(string path)'
    entry.write_text(mission.replace(factory, PROBE + '\n' + factory).replace('return new MissionServer;', 'return new PredateDeploymentValidationMission;'))
    client = world.WORLD / 'client'
    client.mkdir(exist_ok=True)
    (client / 'PredateDeploymentValidation.txt').touch()
    report = dict(profile=profile, passed=False, peer_roster=args.peer_roster, **native_evidence())
    nonce = ''
    try:
        nonce = start_client()
        wait_status(lambda s: s.get('PlayerMagazine') == 30 and s.get('PlayerLife') == 1 and client_state().get('Phase') == 'connected', 'first kit did not load')
        first_death = death(1)
        previous_signal = store.read_json(world.PROFILE / 'PredateWorldState.json')['BotSignal']
        first_raven = store.read_json(world.PROFILE / 'PredateProgress.json')['Operator']
        report['same_session'] = redeploy(2)
        same_raven(first_death, report['same_session'])
        assert store.read_json(world.PROFILE / 'PredateProgress.json')['Operator'] == first_raven
        first_redeployment = store.read_json(world.PROFILE / 'PredateWorldState.json')
        assert first_redeployment['BotSignal']['NextPulseAt'] == previous_signal['NextPulseAt']
        assert first_redeployment['BotSignal']['NextSampleSerial'] == previous_signal['NextSampleSerial']
        assert not first_redeployment['BotSignal']['History']
        assert any(item['EventKey'] == 'validation-previous-life' for item in first_redeployment['Observations']['Entries'])
        assert first_redeployment['Hunter']['EvidenceKey'] != 'validation-previous-life'
        report['previous_observation_preserved_without_old_target_lock'] = True
        print('Passed same-session redeploy with the same wounded operator.', flush=True)

        second_death = death(2)
        close_client(nonce)
        world.stop(make_backup=False)
        saved = store.read_json(world.PROFILE / 'PredateWorldState.json')
        saved_roster = store.read_json(roster_file) if args.peer_roster else None
        assert saved['PlayerAwaitingRedeploy'] and saved['PlayerLife'] == 2
        assert saved['Hunter']['EvidenceKey'] == 'validation-independent-sound'
        nonce = start_client()
        held = wait_status(lambda s: s.get('Players') == 1 and s.get('PlayerSimulationDisabled') and s.get('PlayerMagazine') == -1, 'no staged redeploy after restart')
        assert held['PlayerLife'] == 2 and held['PlayerAwaitingRedeploy'] and held['BotSimulationDisabled'], held
        assert held['SimulationSeconds'] == saved['SimulationSeconds'], held
        if args.peer_roster:
            assert store.read_json(roster_file) == saved_roster
        same_raven(second_death, held)
        report['reopened_death'] = held
        report['after_restart'] = redeploy(3)
        same_raven(second_death, report['after_restart'])
        spawn = report['after_restart']['PlayerPosition']
        bot = report['after_restart']['BotPosition']
        assert sum((spawn[index] - bot[index])**2 for index in (0, 2)) >= 300**2
        if args.peer_roster:
            assert (spawn[0] - 4500)**2 + (spawn[2] - 6900)**2 >= 300**2, spawn
            peers = store.read_json(roster_file)['Peers']
            stable_fields = ('Operator', 'Health', 'Blood', 'Shock', 'Memory', 'Receiver')
            for old, new in zip(saved_roster['Peers'], peers, strict=True):
                assert {key: old[key] for key in stable_fields} == {key: new[key] for key in stable_fields}, (old, new)
            report['alternate_insertion_avoids_living_peers'] = True
            report['peer_state_preserved'] = True
        report['alternate_insertion_kept_raven_position'] = True
        assert store.read_json(world.PROFILE / 'PredateProgress.json')['Operator'] == first_raven
        hunter = store.read_json(world.PROFILE / 'PredateWorldState.json')['Hunter']
        assert hunter['HasGoal'] and hunter['EvidenceKey'] == saved['Hunter']['EvidenceKey'] and hunter['Goal'] == saved['Hunter']['Goal']
        report['independent_investigation_preserved'] = True
        print('Passed opening a saved death and explicitly redeploying.', flush=True)

        death(3, defeat_operator=True)
        report['defeated'] = redeploy(4)
        assert report['defeated']['Defeated']
        assert store.read_json(world.PROFILE / 'PredateProgress.json')['Operator']['StorageKey'] == first_raven['StorageKey']
        logs = '\n'.join(path.read_text(errors='replace') for path in client.glob('script*.log'))
        debriefs = re.findall(r'DEBRIEF: life=(\d+); elapsed=(\d+); bearings=(\d+); defeated=(true|false); fits=(true|false)', logs)
        assert sorted((int(life), defeated, fits) for life, _, _, defeated, fits in debriefs) == [
            (1, 'false', 'true'), (2, 'false', 'true'), (3, 'true', 'true'),
        ], debriefs
        assert any(int(life) == 1 and int(elapsed) >= 1 and int(bearings) == 1 for life, elapsed, bearings, _, _ in debriefs)
        report['debriefs'] = debriefs
        report['passed'] = True
    finally:
        close_client(nonce)
        if world.running():
            world.stop(make_backup=False)
        report['user_preferences_unchanged'] = store.preferences() == original_preferences
        name = 'deployment-roster' if args.peer_roster else 'deployment'
        store.atomic_json(ROOT / f'build/engine-{name}-results.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
