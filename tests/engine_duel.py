"""Stationary opposed native firearm trial; no player save is opened."""
from pathlib import Path
import argparse
import json
import os
import sys
import time
import uuid

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import native_evidence
from engine_loadouts import close_client
import platform_win
import session
import session_store as store
import world


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--range',type=int,choices=(50,150,300),default=150)
    args=parser.parse_args()
    assert not any(platform_win.find_processes(name) for name in ('DayZ_x64.exe','DayZServer_x64.exe','DayZDiag_x64.exe'))
    preferences=store.preferences()
    addon=store.digest(ROOT/'mods/@Predate/addons/Predate.pbo')
    os.environ['PREDATE_CANDIDATE_MOD']='1'
    profile='validation-duel-'+uuid.uuid4().hex[:6]
    info=store.create_operation('Opposed combat validation',profile=profile,extra_mods=[])
    info['kind']='validation'
    store.atomic_json(world.WORLD/'operation.json',info)
    config=store.read_json(world.PROFILE/'PredateConfig.json')
    config.update(EnableFairInformation=True,EnableSharedSignals=False,EnableStrategicHunter=False,
                  PlayerStart=[4700,0,2100],BotStart=[5000,0,2500],ThreatDistance=600,PatrolSize=args.range,
                  BotAccuracyMin=0.8,BotAccuracyMax=0.95)
    if args.range==300: config['PlayerStart']=[5000,0,2100]
    store.atomic_json(world.PROFILE/'PredateConfig.json',config)
    entry=world.MISSION/'init.c'
    probe=(ROOT/'source/validation/duel_probe.c').read_text()
    entry.write_text(entry.read_text().replace('Mission CreateCustomMission(string path)',probe+'\nMission CreateCustomMission(string path)').replace('return new MissionServer;','return new PredateDuelMission;'))
    report=dict(profile=profile,candidate=True,passed=False,range=args.range,
                stationary_opposed_fire=True,accuracy=[config['BotAccuracyMin'],config['BotAccuracyMax']])
    report.update(native_evidence())
    nonce=''
    try:
        world.start()
        nonce=store.read_json(world.STATE)['session_id']
        world.launch_client(nonce,extra_args=['-window','-x=1920','-y=1080'])
        deadline=time.monotonic()+360
        result={}
        while time.monotonic()<deadline:
            assert world.running(),'Opposed-combat server exited'
            failure=session.read_native(world.WORLD/'client/PredateFailure.json')
            assert not failure,failure.get('message')
            result=session.read_native(world.PROFILE/'DuelResult.json')
            if result.get('Checks'): break
            time.sleep(.1)
        report['measurement']=result
        assert result.get('Checks',0)>=16 and result['Failures']==0,result
        assert result['CommittedShotDrainSeconds']>=2,result
        report['passed']=True
    finally:
        close_client(nonce)
        if world.running(): world.stop(make_backup=False)
        report['playable_addon_unchanged']=store.digest(ROOT/'mods/@Predate/addons/Predate.pbo')==addon
        report['user_preferences_unchanged']=store.preferences()==preferences
        store.atomic_json(ROOT/f'build/engine-duel-{args.range}-results.json',report)
        store.atomic_json(ROOT/f'build/engine-duel-{profile}.json',report)
        print(json.dumps(report,indent=2),flush=True)


if __name__=='__main__': main()
