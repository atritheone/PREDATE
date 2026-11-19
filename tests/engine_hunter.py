"""Native candidate hunter checks, optionally including unattended live locomotion."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import fixture, native_evidence
import platform_win
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', action='store_true')
    parser.add_argument('--displace', action='store_true', help='Relocate the unseen human once and require a lawful new-sample recovery')
    parser.add_argument('--site', choices=('baseline','woodland','village','ridge'), default='baseline')
    args = parser.parse_args()
    if args.displace and not args.client: parser.error('--displace requires --client')
    if args.site != 'baseline' and not args.client: parser.error('Terrain-site trials require --client')
    if args.displace and args.site != 'baseline': parser.error('The authored hidden relocation currently requires the baseline site')
    sites = {
        'baseline': ([4500,0,6500], [4500,0,6900]),
        'woodland': ([10800,0,7400], [10800,0,7800]),
        'village': ([6050,0,7650], [6450,0,7650]),
        'ridge': ([10300,0,9100], [10700,0,9100]),
    }
    bot_start, human_start = sites[args.site]
    if any(platform_win.find_processes(name) for name in ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe')):
        raise RuntimeError('Existing game/server retained; close them before candidate validation')
    stable = ROOT/'mods/@Predate/addons/Predate.pbo'
    digest = hashlib.sha256(stable.read_bytes()).hexdigest()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = fixture('crashlab-primitive-send-d1612')
    settings = json.loads((world.PROFILE/'PredateConfig.json').read_text())
    settings.update(EnableFairInformation=True, EnableSharedSignals=True, EnableStrategicHunter=True, ThreatDistance=120, BearingErrorDegrees=1)
    settings.update(BotStart=bot_start, PlayerStart=human_start)
    store.atomic_json(world.PROFILE/'PredateConfig.json', settings)
    entry = world.MISSION/'init.c'
    injection = '''
class PredateHunterValidationMission : PredateValidationMission
{
    ref PredateHunterRuntimeTests m_HunterProbe;
    override void OnInit()
    {
        super.OnInit();
        PredateHunterTests checks = new PredateHunterTests;
        checks.Run();
        PredateSignalTests signals = new PredateSignalTests;
        signals.Run();
        PredateKnowledgeTests information = new PredateKnowledgeTests;
        information.Run();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunHunterProbe, 1000, false);
    }
    void RunHunterProbe()
    {
        if (!m_PredateSession.m_HuntReady || !m_PredateSession.m_HuntBot)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunHunterProbe, 1000, false);
            return;
        }
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            // Physical signal-event fixtures have their own asynchronous client
            // trial. Do not overlap inventory/vehicle actions with this hunt.
            m_HunterProbe = new PredateHunterRuntimeTests;
            m_HunterProbe.Run(m_PredateSession, player);
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunHunterProbe, 1000, false);
    }
}
'''
    marker = 'Mission CreateCustomMission(string path)'
    if args.displace:
        injection = injection.replace('m_HunterProbe = new PredateHunterRuntimeTests;', 'm_HunterProbe = new PredateHunterRuntimeTests;\n            m_HunterProbe.TestHiddenDisplacement = true;')
    setup = (f'm_HunterProbe = new PredateHunterRuntimeTests;\n'
             f'            m_HunterProbe.Site = "{args.site}";\n'
             f'            m_HunterProbe.AuthoredBotStart = "{bot_start[0]} 0 {bot_start[2]}";\n'
             f'            m_HunterProbe.AuthoredHumanStart = "{human_start[0]} 0 {human_start[2]}";')
    injection = injection.replace('m_HunterProbe = new PredateHunterRuntimeTests;', setup)
    entry.write_text(entry.read_text().replace(marker, injection+'\n'+marker).replace('return new PredateValidationMission;', 'return new PredateHunterValidationMission;'), encoding='utf-8')
    report = dict(profile=profile, candidate=True, passed=False, hidden_displacement=args.displace, site=args.site)
    report.update(native_evidence())
    try:
        world.start()
        logs = '\n'.join(p.read_text(errors='replace') for p in world.PROFILE.glob('script*.log'))
        model_failures = [line for line in logs.splitlines() if 'Test] FAIL:' in line]
        assert not model_failures, model_failures
        if args.client:
            nonce = json.loads(world.STATE.read_text())['session_id']
            world.launch_client(nonce, extra_args=['-window', '-x=1920', '-y=1080'])
        marker = '[PredateHunterRuntimeTest] COMPLETE:' if args.client else '[PredateHunterTest] COMPLETE:'
        deadline = time.monotonic() + (520 if args.displace else (350 if args.client else 15))
        next_update = time.monotonic() + 30
        while time.monotonic() < deadline:
            assert world.running(), 'Candidate server exited during the encounter'
            failure_path = world.WORLD/'client/PredateFailure.json'
            if args.client and failure_path.exists():
                raise RuntimeError('Client watchdog caught a failure: '+failure_path.read_text())
            logs = '\n'.join(p.read_text(errors='replace') for p in world.PROFILE.glob('script*.log'))
            if marker in logs: break
            if time.monotonic() >= next_update:
                progress = [line.split('SCRIPT',1)[-1].strip() for line in logs.splitlines() if '[PredateHunterRuntimeTest] PROGRESS:' in line]
                if progress: print(progress[-1], flush=True)
                next_update = time.monotonic()+30
            time.sleep(.25)
        tags = ('[PredateHunterTest]', '[PredateHunterRuntimeTest]', '[PredateSignalTest]', '[PredateSignalServiceTest]', '[PredateKnowledgeTest]')
        checks = [line.split('SCRIPT', 1)[-1].strip() for line in logs.splitlines() if any(tag in line for tag in tags)]
        report['checks'] = checks
        if args.client: report['runtime'] = store.read_json(world.PROFILE/'HunterRuntimeReport.json', {})
        assert any(marker in line and 'failures=0' in line for line in checks), checks
        assert not any('FAIL:' in line for line in checks), checks
        if args.client: assert report['runtime'].get('Reacquired') and report['runtime']['Failures'] == 0, report['runtime']
        report['passed'] = True
    finally:
        if args.client and platform_win.find_processes('DayZ_x64.exe'):
            import session
            status = session.read_native(world.WORLD/'client/PredateClientStatus.json')
            action = 'quit' if world.running() else 'server-closed'
            store.atomic_json(world.WORLD/'client/PredateClientCommand.json', dict(Schema=1, SessionId=json.loads(world.STATE.read_text())['session_id'], Sequence=status.get('Acknowledged',0)+1, Action=action))
            until = time.monotonic()+45
            while platform_win.find_processes('DayZ_x64.exe') and time.monotonic()<until: time.sleep(.25)
        if world.running(): world.stop(make_backup=False)
        if report['passed']:
            state_path = world.PROFILE/'PredateWorldState.json'
            saved = json.loads(state_path.read_text())
            keys = ('HunterSchema', 'Hunter', 'BotSignal', 'Observations', 'SimulationSeconds')
            expected = {key:saved.get(key) for key in keys}
            try:
                world.install_mission()
                world.start()
                loaded = json.loads(state_path.read_text())
                report['headless_reopen_preserves_hunter_state'] = expected == {key:loaded.get(key) for key in keys}
                assert report['headless_reopen_preserves_hunter_state']
            except Exception:
                report['passed'] = False
                raise
            finally:
                if world.running(): world.stop(make_backup=False)
        report['playable_addon_unchanged'] = hashlib.sha256(stable.read_bytes()).hexdigest() == digest
        assert report['playable_addon_unchanged']
        store.atomic_json(ROOT/'build/engine-hunter-results.json', report)
        store.atomic_json(ROOT/f'build/engine-hunter-{profile}.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
