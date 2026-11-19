"""Candidate native signal model/service checks in a disposable world."""
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
    parser.add_argument('--width', type=int, default=1920)
    parser.add_argument('--height', type=int, default=1080)
    parser.add_argument('--fullscreen', action='store_true', help='Use a fullscreen client at the requested resolution')
    parser.add_argument('--skip-reopen', action='store_true', help='Check live behavior without restarting the disposable server')
    args = parser.parse_args()
    if args.width < 1280 or args.height < 720: parser.error('Use at least 1280 by 720 for this UI fixture')
    if any(platform_win.find_processes(name) for name in ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe')):
        raise RuntimeError('Existing game/server retained; candidate test requires closed processes')
    stable = ROOT/'mods/@Predate/addons/Predate.pbo'
    digest = hashlib.sha256(stable.read_bytes()).hexdigest()
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = fixture('crashlab-primitive-send-d1612')
    config = json.loads((world.PROFILE/'PredateConfig.json').read_text())
    config.update(EnableFairInformation=True, EnableSharedSignals=True)
    store.atomic_json(world.PROFILE/'PredateConfig.json', config)
    # The copied prototype save starts locked. Let the probe acquire its own
    # lock after it places both actors, so the receiver sees that sample first.
    progress_path = world.PROFILE/'PredateProgress.json'
    progress = json.loads(progress_path.read_text())
    progress.update(Selected=False, HasPulse=False, HasPulseOrigin=False, PulseRemaining=0)
    store.atomic_json(progress_path, progress)
    entry = world.MISSION/'init.c'
    script = entry.read_text()
    injection = '''
class PredateSignalValidationMission : PredateValidationMission
{
    ref PredateSignalTests m_SignalChecks;
    override void OnInit()
    {
        super.OnInit();
        PredateSignalTests checks = new PredateSignalTests;
        checks.Run();
        PredateFootprintTests footprints = new PredateFootprintTests;
        footprints.Run();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunSignalService, 1000, false);
    }
    void RunSignalService()
    {
        if (!m_PredateSession.m_HuntReady || !m_PredateSession.m_HuntBot)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunSignalService, 1000, false);
            return;
        }
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity()) continue;
            Weapon_Base rifle = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
            if (rifle && !rifle.GetAttachedOptics()) rifle.GetInventory().CreateAttachment("ACOGOptic");
            m_SignalChecks = new PredateSignalTests;
            m_SignalChecks.RunService(m_PredateSession, player);
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunSignalService, 1000, false);
    }
}
'''
    marker = 'Mission CreateCustomMission(string path)'
    script = script.replace(marker, injection+'\n'+marker).replace('return new PredateValidationMission;', 'return new PredateSignalValidationMission;')
    entry.write_text(script, encoding='utf-8')
    report = dict(profile=profile, candidate=True, passed=False, client_resolution=[args.width, args.height] if args.client else None)
    report.update(native_evidence())
    try:
        world.start()
        if args.client:
            nonce = json.loads(world.STATE.read_text())['session_id']
            (world.WORLD/'client').mkdir(exist_ok=True)
            (world.WORLD/'client/PredateReceiverValidation.txt').write_text('Disposable validation client only')
            (world.WORLD/'client/PredatePresentationValidation.txt').write_text('Disposable compass validation only')
            store.atomic_json(world.WORLD/'client/PredatePresentation.json',dict(Schema=1,CompassPercent=100,HighContrast=False))
            video_args = ['-window', f'-x={args.width}', f'-y={args.height}']
            if args.fullscreen:
                video_args = []
                for user in (os.environ['USERNAME'], 'Survivor'):
                    config = world.WORLD/'client/Users'/user/'DayZ.cfg'
                    config.parent.mkdir(parents=True, exist_ok=True)
                    config.write_text(f'Windowed=0;\nWindowWidth={args.width};\nWindowHeight={args.height};\n')
            world.launch_client(nonce, extra_args=video_args+['-predateReceiverValidation','-predatePresentationValidation','-predateScopeValidation'])
        marker = '[PredateSignalServiceTest] COMPLETE:' if args.client else '[PredateSignalTest] COMPLETE:'
        deadline = time.monotonic() + (150 if args.client else 15)
        client = world.WORLD/'client'
        startup_input = ''
        while time.monotonic() < deadline:
            failure = store.read_json(client/'PredateFailure.json')
            if failure:
                raise RuntimeError(f"DayZ {failure['kind']} error: {failure['message']}")
            logs = '\n'.join(p.read_text(errors='replace') for p in world.PROFILE.glob('script*.log'))
            if marker in logs:
                if any('[PredateRivalTest] FAIL' in line for line in logs.splitlines()):
                    break
                if not args.client:
                    break
                if all((client / name).is_file() for name in ('ReceiverValidation.json', 'PresentationProbe.json')):
                    client_logs = '\n'.join(p.read_text(errors='replace') for p in client.glob('script*.log'))
                    startup_input = next((line.strip() for line in client_logs.splitlines() if '[Predate] Startup input ready:' in line), '')
                    if startup_input:
                        break
            time.sleep(.25)
        checks = [line.split('SCRIPT', 1)[-1].strip() for line in logs.splitlines() if '[PredateSignal' in line]
        report['checks'] = checks
        assert any(marker in line and 'failures=0' in line for line in checks), checks
        assert not any('FAIL:' in line for line in checks), checks
        reports = [line.split('SCRIPT', 1)[-1].strip() for line in logs.splitlines() if '[PredateFootprintTest]' in line]
        report['footprint_checks'] = reports
        assert any('checks=10; failures=0' in line for line in reports), reports
        if args.client:
            supply = [line.split('SCRIPT', 1)[-1].strip() for line in logs.splitlines() if '[PredateSupplyTest]' in line]
            report['supply_checks'] = supply
            assert any('checks=19; failures=0' in line for line in supply), supply
            rival = [line.split('SCRIPT', 1)[-1].strip() for line in logs.splitlines() if '[PredateRivalTest]' in line]
            report['rival_checks'] = rival
            assert any('checks=11; failures=0' in line for line in rival), rival
            receiver = json.loads((world.WORLD/'client/ReceiverValidation.json').read_text())
            report['receiver_ui'] = receiver
            assert receiver['Checks'] >= 22 and receiver['Failures'] == 0, receiver
            presentation=json.loads((world.WORLD/'client/PresentationProbe.json').read_text())
            report['presentation_ui']=presentation
            assert presentation['Checks']>=27 and presentation['Failures']==0,presentation
            scoped = store.read_json(client/'ScopedNightVision.json')
            report['scoped_night_vision'] = scoped
            assert scoped and all(scoped[name] for name in ('ScopeOpened','NightVisionApplied','NightVisionCleared')), scoped
            report['startup_input'] = startup_input
            assert 'game focus=true' in report['startup_input'], report['startup_input']
        report['passed'] = True
    finally:
        if args.client and platform_win.find_processes('DayZ_x64.exe'):
            import session
            state = session.read_native(world.WORLD/'client/PredateClientStatus.json')
            action = 'quit' if world.running() else 'server-closed'
            store.atomic_json(world.WORLD/'client/PredateClientCommand.json', dict(Schema=1, SessionId=json.loads(world.STATE.read_text())['session_id'], Sequence=state.get('Acknowledged', 0)+1, Action=action))
            until = time.monotonic()+45
            while platform_win.find_processes('DayZ_x64.exe') and time.monotonic()<until: time.sleep(.25)
        if world.running(): world.stop(make_backup=False)
        if report['passed'] and not args.skip_reopen:
            path = world.PROFILE/'PredateWorldState.json'
            saved = json.loads(path.read_text())
            keys = ('SignalSchema', 'PlayerSignal', 'BotSignal', 'PlayerActivity', 'BotActivity', 'PlayerInterceptionUntil', 'BotInterceptionUntil', 'SimulationSeconds', 'PlayerSupplies')
            expected = {key:saved.get(key) for key in keys}
            progress_path = world.PROFILE/'PredateProgress.json'
            operator_supplies = store.read_json(progress_path)['Operator']['Supplies']
            try:
                world.install_mission()
                world.start()
                loaded = json.loads(path.read_text())
                report['headless_reopen_preserves_signal_state'] = expected == {key:loaded.get(key) for key in keys}
                assert report['headless_reopen_preserves_signal_state']
                report['headless_reopen_preserves_supply_timers'] = bool(saved['PlayerSupplies']['NextAt'] and operator_supplies['NextAt'] and store.read_json(progress_path)['Operator']['Supplies'] == operator_supplies)
                assert report['headless_reopen_preserves_supply_timers']
            except Exception:
                report['passed'] = False
                raise
            finally:
                if world.running(): world.stop(make_backup=False)
        report['playable_addon_unchanged'] = hashlib.sha256(stable.read_bytes()).hexdigest() == digest
        assert report['playable_addon_unchanged']
        store.atomic_json(world.PROFILE/'SignalValidation.json', report)
        store.atomic_json(ROOT/'build/engine-signal-results.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
