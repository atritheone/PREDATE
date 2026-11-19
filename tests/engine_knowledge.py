"""Run information-model checks in real Enforce against the candidate addon only."""
from pathlib import Path
import hashlib
import json
import os
import sys
import time
import argparse

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_sessions import fixture, native_evidence
import platform_win
import session_store as store
import world


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client',action='store_true',help='Use a real selected human controller for the native visibility probe')
    args=parser.parse_args()
    if any(platform_win.find_processes(name) for name in ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe')):
        raise RuntimeError('Existing game/server retained; close them before native candidate validation')
    stable=ROOT/'mods/@Predate/addons/Predate.pbo'
    before=hashlib.sha256(stable.read_bytes()).hexdigest()
    os.environ['PREDATE_CANDIDATE_MOD']='1'
    profile=fixture('crashlab-primitive-send-d1612')
    config=world.PROFILE/'PredateConfig.json'
    settings=json.loads(config.read_text())
    settings['EnableFairInformation']=True
    store.atomic_json(config,settings)
    entry=world.MISSION/'init.c'
    script=entry.read_text()
    injection='''
class PredateKnowledgeValidationMission : PredateValidationMission
{
    ref PredateTacticalKnowledgeTests m_RuntimeChecks;
    override void OnInit()
    {
        super.OnInit();
        PredateKnowledgeTests checks = new PredateKnowledgeTests;
        checks.Run();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunNativeKnowledge, 9000, false);
    }

    void RunNativeKnowledge()
    {
        if (FileExist("$profile:KnowledgeRequiresHuman.txt"))
        {
            bool joined;
            array<Man> players = new array<Man>;
            GetGame().GetPlayers(players);
            eAIBase observerActor = m_PredateSession.m_HuntBot;
            if (observerActor && m_PredateSession.m_ActorsActive)
            {
                observerActor.DisableSimulation(false);
                observerActor.OverrideStance(DayZPlayerConstants.STANCEIDX_ERECT, true);
                vector operatorPose = observerActor.GetBonePositionWS(observerActor.GetBoneIndexByName("neck")) - observerActor.GetPosition();
                foreach (Man participant : players)
                {
                    PlayerBase player = PlayerBase.Cast(participant);
                    if (!player || !player.GetIdentity()) continue;
                    vector playerPose = player.GetBonePositionWS(player.GetBoneIndexByName("neck")) - player.GetPosition();
                    if (operatorPose[1] > 0.7 && playerPose[1] > 0.7) joined = true;
                }
            }
            if (!joined)
            {
                GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RunNativeKnowledge, 1000, false);
                return;
            }
        }
        m_RuntimeChecks = new PredateTacticalKnowledgeTests;
        m_RuntimeChecks.RunNative(m_PredateSession);
    }
}
'''
    marker='Mission CreateCustomMission(string path)'
    assert script.count(marker)==1
    script=script.replace(marker,injection+'\n'+marker).replace('return new PredateValidationMission;','return new PredateKnowledgeValidationMission;')
    entry.write_text(script,encoding='utf-8')
    if args.client:(world.PROFILE/'KnowledgeRequiresHuman.txt').write_text('Selected client required for visibility validation.\n')
    report={'profile':profile,'candidate':True,'passed':False}
    report.update(native_evidence())
    try:
        world.start()
        if args.client:
            identifier=json.loads(world.STATE.read_text())['session_id']
            world.launch_client(identifier,extra_args=['-window','-x=1920','-y=1080'])
        deadline=time.monotonic()+(180 if args.client else 25)
        while time.monotonic()<deadline:
            logs='\n'.join(p.read_text(errors='replace') for p in world.PROFILE.glob('script*.log'))
            if '[PredateTacticalTest] COMPLETE:' in logs:break
            time.sleep(.25)
        observations=[line.split('SCRIPT',1)[-1].strip() for line in logs.splitlines() if '[PredateKnowledgeTest]' in line or '[PredateTacticalTest]' in line]
        report['observations']=observations
        assert any('COMPLETE:' in line and 'failures=0' in line for line in observations),observations
        assert any('[PredateTacticalTest] COMPLETE:' in line and 'failures=0' in line for line in observations),observations
        assert not any('FAIL:' in line for line in observations),observations
        report['passed']=True
    finally:
        if args.client and platform_win.find_processes('DayZ_x64.exe'):
            state=__import__('session').read_native(world.WORLD/'client/PredateClientStatus.json')
            action = 'quit' if world.running() else 'server-closed'
            store.atomic_json(world.WORLD/'client/PredateClientCommand.json',{'Schema':1,'SessionId':json.loads(world.STATE.read_text())['session_id'],'Sequence':state.get('Acknowledged',0)+1,'Action':action})
            until=time.monotonic()+45
            while platform_win.find_processes('DayZ_x64.exe') and time.monotonic()<until:time.sleep(.25)
        if world.running():world.stop(make_backup=False)
        if report['passed']:
            state_path=world.PROFILE/'PredateWorldState.json'
            saved=json.loads(state_path.read_text())
            keys=('InformationSchema','Observations','PendingSounds','SimulationSeconds')
            expected={key:saved.get(key) for key in keys}
            try:
                world.install_mission()
                world.start()
                restored=json.loads(state_path.read_text())
                actual={key:restored.get(key) for key in keys}
                report['headless_reopen_preserves_private_information']=actual==expected
                assert actual==expected,'Private information changed without an active human'
            except Exception:
                report['passed']=False
                raise
            finally:
                if world.running():world.stop(make_backup=False)
        report['playable_addon_unchanged']=hashlib.sha256(stable.read_bytes()).hexdigest()==before
        assert report['playable_addon_unchanged']
        store.atomic_json(ROOT/'build/engine-knowledge-results.json',report)
        store.atomic_json(ROOT/f'build/engine-knowledge-{profile}.json',report)
        print(json.dumps(report,indent=2),flush=True)


if __name__=='__main__':main()
