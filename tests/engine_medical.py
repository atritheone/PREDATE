"""Measure native self-bandaging and strategic handoff in a disposable hunt."""
from pathlib import Path
import json
import os
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from engine_loadouts import close_client
from engine_sessions import native_evidence
import platform_win
import session
import session_store as store
import world


PROBE = r'''
class PredateMedicalReport
{
    int Schema = 1;
    int Checks;
    int Failures;
    float InitialDressings;
    float RemainingDressings;
    int InitialAmmo;
    int RemainingAmmo;
    float BandagingSeconds;
    float BotHealth;
    float BotBlood;
    bool SawNativeBandageAction;
    int MinAmmoDuringBandaging = -1;
    bool RouteYielded;
    bool RouteResumed;
    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateMedicalTest] FAIL: " + label);
    }
}

class PredateMedicalMission : MissionServer
{
    ref PredateMedicalReport m_MedicalReport = new PredateMedicalReport;
    eAIBase m_OriginalMedicalActor;
    float m_MedicalBegan;
    float m_WoundAt;
    float m_TreatedAt;
    float m_NextMedicalCheck;
    bool m_MedicalDone;
    bool m_MedicalSeeded;
    ref PredateCombatRuntimeTests m_MedicalAmmoMeter = new PredateCombatRuntimeTests;

    float DressingQuantity(eAIBase actor)
    {
        array<EntityAI> items = new array<EntityAI>;
        actor.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
        float quantity;
        foreach (EntityAI item : items)
        {
            ItemBase dressing = ItemBase.Cast(item);
            if (dressing && dressing.GetType() == "BandageDressing") quantity += dressing.GetQuantity();
        }
        return quantity;
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        float now = GetGame().GetTickTime();
        if (m_MedicalDone || now < m_NextMedicalCheck) return;
        m_NextMedicalCheck = now + 0.05;
        eAIBase actor = m_PredateSession.m_HuntBot;
        if (!actor || !m_PredateSession.m_HuntReady) return;
        if (FileExist("$profile:MedicalInitial.json"))
        {
            CheckMedicalRestore(actor);
            return;
        }
        if (!m_PredateSession.m_ActorsActive) return;
        if (m_MedicalBegan == 0) m_MedicalBegan = now;
        if (!m_MedicalSeeded)
        {
            if (!actor.m_PredateRouteActive)
            {
                if (now - m_MedicalBegan > 90)
                {
                    m_MedicalReport.Check(false, "a real strategic route is active before the wound");
                    FinishMedical(actor);
                }
                return;
            }
            m_OriginalMedicalActor = actor;
            m_MedicalReport.Check(true, "a real strategic route is active before the wound");
            PredateCombatRuntimeTests meter = new PredateCombatRuntimeTests;
            int initialAmmo = meter.CountAmmo(actor);
            float initialDressings = DressingQuantity(actor);
            m_MedicalReport.InitialAmmo = initialAmmo;
            m_MedicalReport.InitialDressings = initialDressings;
            actor.SetHealth("", "Health", 80);
            actor.SetHealth("", "Blood", 4000);
            bool wounded = actor.GetBleedingManagerServer().AttemptAddBleedingSourceBySelection("LeftForeArm");
            m_MedicalReport.Check(wounded && actor.IsBleeding(), "the native wound is actually bleeding");
            m_MedicalReport.Check(initialDressings > 0, "the actor owns finite medical supplies");
            m_MedicalSeeded = true;
            m_WoundAt = now;
        }
        if (ActionBandageSelf.Cast(actor.GetActionManager().GetRunningAction()))
        {
            m_MedicalReport.SawNativeBandageAction = true;
            int actualAmmo = m_MedicalAmmoMeter.CountAmmo(actor);
            if (m_MedicalReport.MinAmmoDuringBandaging < 0 || actualAmmo < m_MedicalReport.MinAmmoDuringBandaging)
                m_MedicalReport.MinAmmoDuringBandaging = actualAmmo;
        }
        if (actor.IsBleeding() && !actor.m_PredateRouteActive && !actor.m_PredateStrategicControl) m_MedicalReport.RouteYielded = true;
        if (!actor.IsBleeding() && m_TreatedAt == 0)
        {
            m_TreatedAt = now;
            m_MedicalReport.BandagingSeconds = now - m_WoundAt;
        }
        if (m_TreatedAt > 0 && actor.m_PredateRouteActive && !actor.GetActionManager().GetRunningAction())
        {
            m_MedicalReport.RouteResumed = true;
            FinishMedical(actor);
        }
        else if (now - m_WoundAt > 120)
        {
            m_MedicalReport.Check(false, "native treatment and resumed hunting finish within the fixture deadline");
            FinishMedical(actor);
        }
    }

    void FinishMedical(eAIBase actor)
    {
        m_MedicalDone = true;
        PredateCombatRuntimeTests meter = new PredateCombatRuntimeTests;
        int remainingAmmo = meter.CountAmmo(actor);
        float remainingDressings = DressingQuantity(actor);
        float health = actor.GetHealth("", "Health");
        float blood = actor.GetHealth("", "Blood");
        m_MedicalReport.RemainingAmmo = remainingAmmo;
        m_MedicalReport.RemainingDressings = remainingDressings;
        m_MedicalReport.BotHealth = health;
        m_MedicalReport.BotBlood = blood;
        m_MedicalReport.Check(actor == m_OriginalMedicalActor && actor.IsAlive(), "treatment retains the same living actor");
        m_MedicalReport.Check(m_MedicalReport.SawNativeBandageAction, "the actual native self-bandaging action ran");
        m_MedicalReport.Check(!actor.IsBleeding(), "the native wound is treated");
        m_MedicalReport.Check(remainingDressings == m_MedicalReport.InitialDressings - 1, "one native dressing use is consumed without resupply");
        m_MedicalReport.Check(remainingAmmo == m_MedicalReport.InitialAmmo, "treatment does not replenish or consume ammunition");
        m_MedicalReport.Check(m_MedicalReport.MinAmmoDuringBandaging == m_MedicalReport.InitialAmmo, "ammunition in the stowed rifle remains accounted for while a dressing is held");
        m_MedicalReport.Check(m_MedicalReport.RouteYielded && m_MedicalReport.RouteResumed, "strategic travel yields to treatment then resumes");
        m_MedicalReport.Check(health > 0 && health < 99 && blood > 0 && blood < 4990, "treatment does not silently refill health and blood");
        m_PredateSession.PredateSave();
        string error;
        JsonFileLoader<PredateMedicalReport>.SaveFile("$profile:MedicalInitial.json", m_MedicalReport, error);
        Print("[PredateMedicalTest] COMPLETE: checks=" + m_MedicalReport.Checks + "; failures=" + m_MedicalReport.Failures);
    }

    void CheckMedicalRestore(eAIBase actor)
    {
        m_MedicalDone = true;
        PredateMedicalReport initial = new PredateMedicalReport;
        string error;
        bool loaded = JsonFileLoader<PredateMedicalReport>.LoadFile("$profile:MedicalInitial.json", initial, error);
        m_MedicalReport.Check(loaded, "the initial treatment report is readable");
        if (loaded)
        {
            float quantity = DressingQuantity(actor);
            m_MedicalReport.RemainingDressings = quantity;
            m_MedicalReport.Check(quantity == initial.RemainingDressings, "spent medical supplies remain spent after a fresh process");
            m_MedicalReport.Check(!actor.IsBleeding(), "the treated wound remains treated after continuation");
        }
        JsonFileLoader<PredateMedicalReport>.SaveFile("$profile:MedicalReopened.json", m_MedicalReport, error);
    }
}
'''


def main():
    assert not any(platform_win.find_processes(name) for name in
                   ('DayZ_x64.exe','DayZServer_x64.exe','DayZDiag_x64.exe'))
    preferences = store.preferences()
    addon = store.digest(ROOT/'mods/@Predate/addons/Predate.pbo')
    os.environ['PREDATE_CANDIDATE_MOD'] = '1'
    profile = 'validation-medical-'+uuid.uuid4().hex[:6]
    info = store.create_operation('Medical validation', profile=profile, extra_mods=[])
    info['kind'] = 'validation'
    store.atomic_json(world.WORLD/'operation.json', info)
    config = store.read_json(world.PROFILE/'PredateConfig.json')
    config.update(PlayerStart=[4500,0,6500], BotStart=[5500,0,6500], ThreatDistance=150)
    store.atomic_json(world.PROFILE/'PredateConfig.json', config)
    entry = world.MISSION/'init.c'
    entry.write_text(entry.read_text().replace('Mission CreateCustomMission(string path)', PROBE+'\nMission CreateCustomMission(string path)').replace('return new MissionServer;', 'return new PredateMedicalMission;'))
    report = dict(profile=profile, candidate=True, passed=False)
    report.update(native_evidence())
    nonce = ''
    try:
        world.start()
        nonce = store.read_json(world.STATE)['session_id']
        world.launch_client(nonce, extra_args=['-window','-x=1920','-y=1080'])
        for filename, limit, checks in [('MedicalInitial.json',270,11),('MedicalReopened.json',20,3)]:
            if filename == 'MedicalReopened.json': world.start()
            deadline = time.monotonic()+limit
            path = world.PROFILE/filename
            while not path.exists() and time.monotonic()<deadline:
                assert world.running(), 'Medical-validation server exited'
                failure = session.read_native(world.WORLD/'client/PredateFailure.json')
                assert not failure, failure.get('message')
                time.sleep(.1)
            result = store.read_json(path, {})
            report[filename.removesuffix('.json')] = result
            assert result.get('Checks',0) >= checks and result['Failures'] == 0, result
            close_client(nonce)
            world.stop(make_backup=False)
        report['passed'] = True
    finally:
        close_client(nonce)
        if world.running(): world.stop(make_backup=False)
        report['playable_addon_unchanged'] = store.digest(ROOT/'mods/@Predate/addons/Predate.pbo') == addon
        report['user_preferences_unchanged'] = preferences == store.preferences()
        store.atomic_json(ROOT/'build/engine-medical-results.json', report)
        store.atomic_json(ROOT/f'build/engine-medical-{profile}.json', report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__': main()
