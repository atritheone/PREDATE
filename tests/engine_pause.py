"""Real local pause, expected timeout recovery and save-from-pause checks."""
from pathlib import Path
import argparse
import json
import hashlib
import os
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from engine_sessions import Bridge, fixture, native_evidence
from engine_lifecycle import native_command
import platform_win
import session
import session_store as store
import world
import world_pause


def client_status(): return session.read_native(world.WORLD/'client/PredateClientStatus.json')


def wait_client(predicate, timeout=15):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        value=client_status()
        if value.get('Error'): raise RuntimeError(value['Error'])
        if value and predicate(value): return value
        time.sleep(.1)
    raise TimeoutError('Client pause state did not settle: '+str(client_status()))


def wait_server(predicate, timeout=15):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        value=world.native_status()
        if value.get('Error'): raise RuntimeError(value['Error'])
        if value and predicate(value): return value
        time.sleep(.1)
    raise TimeoutError('Server activation did not settle: '+str(world.native_status()))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds',type=int,default=35)
    parser.add_argument('--candidate',action='store_true',help='Use source supervision and the opt-in candidate hunter')
    args=parser.parse_args()
    assert not platform_win.find_processes('DayZ_x64.exe') and not platform_win.find_processes('DayZServer_x64.exe')
    preferences=store.preferences()
    if args.candidate:
        if session.manager_alive(): raise RuntimeError('Existing supervisor retained; candidate validation requires a fresh source supervisor')
        os.environ['PREDATE_CANDIDATE_MOD']='1'
    addon=store.ROOT/'mods/@Predate/addons/Predate.pbo'
    addon_hash=hashlib.sha256(addon.read_bytes()).hexdigest()
    profile=fixture('crashlab-primitive-send-d1612')
    if args.candidate:
        path=world.PROFILE/'PredateConfig.json'
        config=json.loads(path.read_text())
        config.update(EnableFairInformation=True,EnableSharedSignals=True,EnableStrategicHunter=True,
                      BotAccuracyMin=0,BotAccuracyMax=0)
        store.atomic_json(path,config)
    bridge=Bridge(source=args.candidate,candidate=args.candidate)
    report={'profile':profile,'passed':False,'candidate':args.candidate}
    report.update(native_evidence())
    leave_open=False
    try:
        bridge.wait_action(bridge.action('continue',profile=profile))
        bridge.wait_phase({'playing'})
        identity={pid:world.process_identity(pid) for pid in platform_win.find_processes('DayZ_x64.exe')}
        # Short pause should return without reconnecting or restarting processes.
        native_command('pause')
        bridge.wait_phase({'paused'},15)
        assert world_pause.suspended(session.state()['session_id'])
        time.sleep(.2)
        before=world.native_status()
        assert before['Paused'] and before['PlayerSimulationDisabled']
        assert before['BotSimulationDisabled']
        time.sleep(2)
        after=world.native_status()
        if after != before:
            report['short_pause_drift']={k:[before.get(k),after.get(k)] for k in before.keys()|after.keys() if before.get(k)!=after.get(k)}
        assert after==before, report
        native_command('request-resume')
        bridge.wait_phase({'playing'},30)
        wait_client(lambda value:not value.get('Paused') and not value.get('PauseMenu'))
        assert not world.native_status()['Paused']
        assert world.native_status()['PlayerDamageAllowed']
        report['short_pause_resume']=True
        print('PASS: short pause holds and resumes',flush=True)
        native_command('pause')
        bridge.wait_phase({'paused'},15)
        time.sleep(.2)
        before=world.native_status()
        started=time.monotonic()
        while time.monotonic()-started<args.seconds:
            phase=bridge.call('status')['phase']
            if phase in ('playing','connecting') and client_status().get('Paused')==0:
                # A person resumed the game. Do not make that look like a crash
                # by closing their session from this test's cleanup handler.
                leave_open=True
                report['interrupted_by_manual_resume']=True
                print('Manual Resume detected; leaving DayZ open under supervision.',flush=True)
                return
            assert world.running() and phase=='paused', {
                'phase':phase,'state':bridge.call('status'),
                'native':world.native_status(),'client':client_status()}
            after=world.native_status()
            if after and after!=before:
                report['long_pause_drift']={k:[before.get(k),after.get(k)] for k in before.keys()|after.keys() if before.get(k)!=after.get(k)}
                raise AssertionError('Server advanced while paused: '+str(report['long_pause_drift']))
            current=client_status()
            assert not current.get('Error'), current
            time.sleep(.2)
        report['paused_seconds']=round(time.monotonic()-started,2)
        current=wait_client(lambda value:value.get('PauseMenu'))
        report['pause_menu_stayed_responsive']=bool(current.get('PauseMenu'))
        report['connection_preserved']=not bool(current.get('PauseDisconnected')) and current.get('Phase')=='connected'
        assert report['connection_preserved'], current
        native_command('request-resume')
        bridge.wait_phase({'playing'},150)
        wait_client(lambda value:not value.get('PauseMenu') and value.get('Phase')=='connected')
        assert all(world.process_identity(pid)==saved for pid,saved in identity.items())
        resumed=wait_server(lambda value:not value.get('Paused') and value.get('PlayerDamageAllowed') and not value.get('PlayerSimulationDisabled'))
        report['resume_vital_deltas']={key:resumed[key]-before[key] for key in ('PlayerHealth','PlayerBlood','BotHealth','BotBlood')}
        assert all(delta<=(10 if 'Blood' in key else .5) for key,delta in report['resume_vital_deltas'].items()),report['resume_vital_deltas']
        report['same_client_resumed_after_long_pause']=True
        print('PASS: long pause resumes without reconnect',flush=True)
        native_command('pause')
        bridge.wait_phase({'paused'},15)
        bridge.wait_action(bridge.action('quit'),90)
        bridge.wait_phase({'suspended'})
        assert not world.running() and not platform_win.find_processes('DayZ_x64.exe')
        assert not world_pause.lease_path().exists()
        report['save_quit_while_paused']=True
        bridge.wait_action(bridge.action('continue',profile=profile))
        bridge.wait_phase({'playing'})
        reopened=wait_server(lambda value:value.get('PlayerDamageAllowed') and not value.get('PlayerSimulationDisabled') and not value.get('BotSimulationDisabled'))
        assert reopened['PlayerDamageAllowed']
        report['reopen_after_paused_save_restores_gameplay']=True
        bridge.wait_action(bridge.action('quit'),90)
        bridge.wait_phase({'suspended'})
        report['passed']=True
    finally:
        try:
            if not leave_open and (world.running() or platform_win.find_processes('DayZ_x64.exe')):
                bridge.wait_action(bridge.action('quit'),90)
        finally:
            try:bridge.close()
            finally:
                store.save_preferences({k:v for k,v in preferences.items() if k!='schema'})
                report['playable_addon_unchanged']=hashlib.sha256(addon.read_bytes()).hexdigest()==addon_hash
                store.atomic_json(store.ROOT/'build/engine-pause-results.json',report)
                store.atomic_json(store.ROOT/f'build/engine-pause-{profile}.json',report)
                print(json.dumps(report,indent=2),flush=True)


if __name__=='__main__': main()
