"""Isolated DayZDiag singleplayer AI-update probe; Steam preferences stay intact."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import platform_win
import session
import session_store as store
import world


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pause',action='store_true')
    parser.add_argument('--hold-actors',action='store_true',help='Explicitly hold local actors while retaining the live menu')
    parser.add_argument('--offline',action='store_true',help='Temporarily test cached Steam Offline Mode and restore the original preference')
    args=parser.parse_args()
    assert not any(platform_win.find_processes(name) for name in ('DayZ_x64.exe','DayZServer_x64.exe','DayZDiag_x64.exe'))
    executable = world.CLIENT/'DayZDiag_x64.exe'
    assert executable.is_file()
    stable = ROOT/'mods/@Predate/addons/Predate.pbo'
    digest = hashlib.sha256(stable.read_bytes()).hexdigest()
    folder = ROOT/'work'/('singleplayer-ai-'+uuid.uuid4().hex[:6])
    mission = folder/'probe.chernarusplus'
    profile = folder/'profile'
    mission.mkdir(parents=True)
    profile.mkdir()
    script=(ROOT/'source/validation/singleplayer_probe.c').read_text()
    holds=(ROOT/'source/validation/singleplayer_hold.c').read_text() if args.hold_actors else ''
    script=script.replace('/*PREDATE_HOLD_METHODS*/',holds)
    (mission/'init.c').write_text(script)
    mods = [ROOT/'mods'/name for name in ('@CF','@DabsFramework','@ExpansionCore','@ExpansionAI','@PredateCandidate')]
    identity = None
    report = dict(singleplayer_probe=True, passed=False, actor_hold=args.hold_actors, steam_preferences_changed=False, folder=folder.relative_to(ROOT).as_posix())
    nonce=uuid.uuid4().hex
    sequence=0
    steam_config=None
    original_steam=None
    def command(action):
        nonlocal sequence
        sequence+=1
        store.atomic_json(profile/'PredateClientCommand.json',dict(Schema=1,SessionId=nonce,Sequence=sequence,Action=action))
    def wait_native(predicate,timeout=15):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            value=session.read_native(profile/'OfflineProbe.json')
            if value and predicate(value):return value
            time.sleep(.1)
        raise TimeoutError('Singleplayer state did not settle: '+str(value))
    try:
        if args.offline:
            from engine_offline import STEAM, steam, stop_steam
            steam_config=STEAM/'config/loginusers.vdf'
            original_steam=steam_config.read_bytes()
            offline,count=re.subn(rb'("WantsOfflineMode"\s*")0(")',rb'\g<1>1\2',original_steam)
            assert count==1,'Expected one cached online account preference'
            stop_steam()
            steam_config.write_bytes(offline)
            steam(['-silent'])
            time.sleep(12)
            assert re.findall(rb'"WantsOfflineMode"\s*"([01])"',steam_config.read_bytes())==[b'1']
            report['steam_offline_preference_active']=True
            report['steam_preferences_changed']=True
        with (folder/'process.log').open('wb') as log:
            process = subprocess.Popen([str(executable),'-mission='+str(mission),'-profiles='+str(profile),
                '-mod='+';'.join(map(str,mods)),'-predateSessionId='+nonce,'-window','-x=1920','-y=1080','-noSplash','-noPause','-doLogs'],
                cwd=world.CLIENT,stdout=log,stderr=subprocess.STDOUT)
        identity = world.process_identity(process.pid)
        assert identity
        world.guard_process(identity,profile,profile/'PredateFailure.json')
        until = time.monotonic()+110
        tested_pause=False
        while process.poll() is None and time.monotonic()<until:
            state=session.read_native(profile/'OfflineProbe.json')
            if args.pause and not tested_pause and state.get('BotTravel',0)>20:
                tested_pause=True
                active_pose=state.get('BotBoneRelative')
                time.sleep(.4)
                next_pose=session.read_native(profile/'OfflineProbe.json').get('BotBoneRelative')
                report['animation_changes_while_active']=active_pose!=next_pose
                command('pause')
                wait_native(lambda value:value.get('EnginePaused') and value.get('PauseMenu'))
                time.sleep(.3)
                held=wait_native(lambda value:value.get('EnginePaused') and len(value.get('BotPosition',[]))==3 and len(value.get('BotBoneRelative',[]))==3)
                time.sleep(4)
                after=wait_native(lambda value:value.get('EnginePaused') and len(value.get('BotPosition',[]))==3 and len(value.get('BotBoneRelative',[]))==3)
                report['native_pause_holds_position']=held.get('BotPosition')==after.get('BotPosition')
                report['native_pause_holds_animation']=held.get('BotBoneRelative')==after.get('BotBoneRelative')
                client=session.read_native(profile/'PredateClientStatus.json')
                report['pause_ui_acknowledged']=client.get('Acknowledged')==sequence and bool(client.get('PauseMenu'))
                report['pause_native_clock_delta']=after.get('NativeClock',0)-held.get('NativeClock',0)
                command('resume')
                resumed=wait_native(lambda value:not value.get('EnginePaused') and not value.get('PauseMenu'))
                time.sleep(1)
                later=session.read_native(profile/'OfflineProbe.json')
                report['native_resume_moves_again']=later.get('BotPosition')!=resumed.get('BotPosition')
            time.sleep(.25)
        status = session.read_native(profile/'OfflineProbe.json')
        report['native'] = status
        report['passed'] = bool(status.get('RuntimeLoaded') and status.get('LocalPlayer') and status.get('Server') and
            not status.get('Multiplayer') and status.get('BotAlive') and status.get('BotTravel',0)>20)
        if args.pause:
            report['passed']=report['passed'] and all(report.get(key) for key in ('animation_changes_while_active','native_pause_holds_position','native_pause_holds_animation','pause_ui_acknowledged','native_resume_moves_again'))
        failure = session.read_native(profile/'PredateFailure.json')
        if failure: report['failure'] = failure.get('message')
    finally:
        if identity and world.process_identity(identity['pid'])==identity:
            # This mission has no persistent character/world. Stop only its
            # verified owned diagnostic process after its bounded experiment.
            command('server-closed')
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                if world.process_identity(identity['pid'])==identity:os.kill(identity['pid'],signal.SIGTERM)
                process.wait(timeout=10)
        report['playable_addon_unchanged'] = hashlib.sha256(stable.read_bytes()).hexdigest()==digest
        assert report['playable_addon_unchanged']
        if original_steam is not None:
            stop_steam()
            steam_config.write_bytes(original_steam)
            steam(['-silent'])
            report['steam_preferences_restored']=True
        store.atomic_json(ROOT/'build/singleplayer-ai-probe.json',report)
        store.atomic_json(ROOT/('build/singleplayer-'+('pause' if args.pause else 'movement')+'-probe.json'),report)
        if args.offline:store.atomic_json(ROOT/'build/singleplayer-offline-probe.json',report)
        print(json.dumps(report,indent=2),flush=True)


if __name__=='__main__': main()
