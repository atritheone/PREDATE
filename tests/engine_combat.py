"""Measure candidate native shooting in a disposable clear-lane fixture."""
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
import session
import session_store as store
import world


def check_empty_ammo_reopen(report):
    world.install_mission()
    entry = world.MISSION/'init.c'
    injection = '''
class PredateEmptyAmmoReloadMission : MissionServer
{
    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckRestoredAmmo, 500, false);
    }
    void CheckRestoredAmmo()
    {
        if (!m_PredateSession.m_HuntReady || !m_PredateSession.m_HuntBot)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckRestoredAmmo, 500, false);
            return;
        }
        PredateCombatRuntimeTests meter = new PredateCombatRuntimeTests;
        int ammo = meter.CountAmmo(m_PredateSession.m_HuntBot);
        PredateCombatProbeReport restored = new PredateCombatProbeReport;
        restored.RemainingAmmo = ammo;
        restored.Checks = 1;
        if (ammo != 0) restored.Failures = 1;
        string error;
        JsonFileLoader<PredateCombatProbeReport>.SaveFile("$profile:EmptyAmmoRestore.json", restored, error);
    }
}
'''
    entry.write_text(entry.read_text().replace('Mission CreateCustomMission(string path)', injection+'\nMission CreateCustomMission(string path)').replace('return new MissionServer;', 'return new PredateEmptyAmmoReloadMission;'))
    try:
        world.start()
        path = world.PROFILE/'EmptyAmmoRestore.json'
        deadline = time.monotonic()+15
        while not path.exists() and time.monotonic()<deadline: time.sleep(.1)
        restored = store.read_json(path, {})
        assert restored.get('Checks') == 1 and restored['Failures'] == 0, restored
        report['empty_ammo_reopen_total'] = restored['RemainingAmmo']
        report['empty_ammo_preserved_after_reopen'] = True
    finally:
        if world.running(): world.stop(make_backup=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--range', type=int, choices=(50, 150, 300), default=150)
    parser.add_argument('--accuracy-min', type=float)
    parser.add_argument('--accuracy-max', type=float)
    parser.add_argument('--moving', action='store_true', help='Use an independent target moving through native AI navigation')
    parser.add_argument('--rounds', type=int, default=0, help='Measure complete depletion of a 1-10 round native ammunition budget')
    outcome = parser.add_mutually_exclusive_group()
    outcome.add_argument('--exposure', action='store_true', help='Also test an exposed head above real chest cover')
    outcome.add_argument('--lethal', action='store_true', help='Allow normal damage and require an actual target death')
    outcome.add_argument('--damage', action='store_true', help='Allow normal damage and require a real wound, reporting death separately')
    args = parser.parse_args()
    if args.rounds and (not 1 <= args.rounds <= 10 or args.moving or args.exposure or args.lethal or args.damage):
        parser.error('--rounds uses 1-10 rounds and the isolated stationary, protected resource fixture')
    if any(platform_win.find_processes(name) for name in
           ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe')):
        raise RuntimeError('Existing game/server retained; combat validation requires closed processes')
    stable = ROOT/'mods/@Predate/addons/Predate.pbo'
    digest = hashlib.sha256(stable.read_bytes()).hexdigest()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = fixture('crashlab-primitive-send-d1612')
    config = json.loads((world.PROFILE/'PredateConfig.json').read_text())
    config.update(EnableFairInformation=True, EnableSharedSignals=True, EnableStrategicHunter=False,
                  ThreatDistance=600, BotStart=[5000, 0, 2500], PatrolSize=args.range)
    if args.accuracy_min is not None: config['BotAccuracyMin'] = args.accuracy_min
    if args.accuracy_max is not None: config['BotAccuracyMax'] = args.accuracy_max
    assert 0 <= config['BotAccuracyMin'] <= config['BotAccuracyMax'] <= 1
    store.atomic_json(world.PROFILE/'PredateConfig.json', config)
    entry = world.MISSION/'init.c'
    injection = '''
class PredateCombatValidationMission : PredateValidationMission
{
    ref PredateCombatRuntimeTests m_CombatProbe;
    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunCombatProbe, 1000, false);
    }
    void RunCombatProbe()
    {
        if (!m_PredateSession.m_HuntReady || !m_PredateSession.m_HuntBot || !m_PredateSession.m_ActorsActive)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunCombatProbe, 1000, false);
            return;
        }
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            m_CombatProbe = new PredateCombatRuntimeTests;
            m_CombatProbe.Run(m_PredateSession, player);
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunCombatProbe, 1000, false);
    }
}
'''
    marker = 'Mission CreateCustomMission(string path)'
    if args.exposure:
        injection = injection.replace('m_CombatProbe = new PredateCombatRuntimeTests;', 'm_CombatProbe = new PredateCombatRuntimeTests;\n            m_CombatProbe.TestExposure = true;')
    if args.lethal:
        injection = injection.replace('m_CombatProbe = new PredateCombatRuntimeTests;', 'm_CombatProbe = new PredateCombatRuntimeTests;\n            m_CombatProbe.TestLethal = true;')
    if args.damage:
        injection = injection.replace('m_CombatProbe = new PredateCombatRuntimeTests;', 'm_CombatProbe = new PredateCombatRuntimeTests;\n            m_CombatProbe.TestDamage = true;')
    if args.moving:
        injection = injection.replace('m_CombatProbe = new PredateCombatRuntimeTests;', 'm_CombatProbe = new PredateCombatRuntimeTests;\n            m_CombatProbe.TestMoving = true;')
    if args.rounds:
        injection = injection.replace('m_CombatProbe = new PredateCombatRuntimeTests;', f'm_CombatProbe = new PredateCombatRuntimeTests;\n            m_CombatProbe.RoundBudget = {args.rounds};')
    entry.write_text(entry.read_text().replace(marker, injection+'\n'+marker).replace('return new PredateValidationMission;', 'return new PredateCombatValidationMission;'), encoding='utf-8')
    report = dict(profile=profile, candidate=True, range=args.range, passed=False,
                  accuracy=[config['BotAccuracyMin'], config['BotAccuracyMax']], round_budget=args.rounds)
    report.update(native_evidence())
    try:
        world.start()
        nonce = json.loads(world.STATE.read_text())['session_id']
        world.launch_client(nonce, extra_args=['-window', '-x=1920', '-y=1080'])
        deadline = time.monotonic()+(360 if args.lethal else 180)
        while time.monotonic()<deadline:
            assert world.running(), 'Candidate server exited during combat validation'
            failure = world.WORLD/'client/PredateFailure.json'
            if failure.exists(): raise RuntimeError(failure.read_text())
            logs = '\n'.join(p.read_text(errors='replace') for p in world.PROFILE.glob('script*.log'))
            if '[PredateCombatTest] COMPLETE:' in logs: break
            time.sleep(.25)
        checks = [line.split('SCRIPT',1)[-1].strip() for line in logs.splitlines() if '[PredateCombatTest]' in line]
        report['checks'] = checks
        result = session.read_native(world.PROFILE/'CombatProbeResult.json')
        report['measurement'] = result
        report['native_entities_during_shot'] = session.read_native(world.PROFILE/'PredateProbeEntities.json')
        assert result and result.get('Failures') == 0, checks
        report['passed'] = True
    finally:
        if platform_win.find_processes('DayZ_x64.exe'):
            status = session.read_native(world.WORLD/'client/PredateClientStatus.json')
            action='quit' if world.running() else 'server-closed'
            store.atomic_json(world.WORLD/'client/PredateClientCommand.json', dict(Schema=1, SessionId=json.loads(world.STATE.read_text())['session_id'], Sequence=status.get('Acknowledged',0)+1, Action=action))
            until = time.monotonic()+45
            while platform_win.find_processes('DayZ_x64.exe') and time.monotonic()<until: time.sleep(.25)
        if world.running(): world.stop(make_backup=False)
        if args.rounds and report['passed']:
            try: check_empty_ammo_reopen(report)
            except Exception as error:
                report['passed'] = False
                report['reopen_error'] = str(error)
        report['playable_addon_unchanged'] = hashlib.sha256(stable.read_bytes()).hexdigest() == digest
        assert report['playable_addon_unchanged']
        store.atomic_json(ROOT/f'build/engine-combat-{profile}.json', report)
        store.atomic_json(ROOT/f'build/engine-combat-{args.range}-results.json', report)
        summary = {key:value for key,value in report.items() if key not in ('checks', 'measurement', 'native_entities_during_shot')}
        summary['checks'] = [line for line in report.get('checks', []) if any(tag in line for tag in ('START:', 'FAIL:', 'MEASURE:', 'COMPLETE:'))]
        summary['measurement'] = {key:value for key,value in report.get('measurement', {}).items() if not isinstance(value, list)}
        print(json.dumps(summary, indent=2), flush=True)
        if report.get('reopen_error'): raise AssertionError(report['reopen_error'])


if __name__ == '__main__': main()
