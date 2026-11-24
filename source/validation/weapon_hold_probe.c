// Disposable mission for the retail CZ-TSR timeout seen on the death screen.
class PredateWeaponHoldProbe : MissionServer
{
    int m_ProbeStage;
    float m_ProbeAt;
    float m_ProbeRemaining;
    int m_ProbeRounds;
    eAIBase m_ProbeActor;
    Weapon_Base m_ProbeRifle;
    DayZPlayerInventory m_ProbeInventory;

    void Report(string text)
    {
        Print("[PredateWeaponHoldTest] " + text);
        if (text == "PASS" || text.IndexOf("FAIL") == 0)
        {
            FileHandle report = OpenFile("$profile:WeaponHold.txt", FileMode.WRITE);
            if (report)
            {
                FPrintln(report, text);
                CloseFile(report);
            }
            m_ProbeStage = 99;
        }
    }

    bool QueueCycle(PlayerBase player, float timeout)
    {
        // Begin a real native shot, with an AI target and a loaded weapon.
        bool created;
        eAITarget contact = player.GetTargetInformation().AddAI(m_ProbeActor, 120000, true, created);
        contact.SetInitial(0.8, player.GetPosition());
        array<ref eAITarget> targets = m_ProbeActor.GetTargets();
        targets.Clear();
        targets.Insert(contact);
        m_ProbeActor.AimAtPosition(player.GetPosition() + "0 1.3 0");
        m_ProbeRifle.ProcessWeaponEvent(new WeaponEventTrigger(m_ProbeActor, null));
        if (!WeaponFire.Cast(m_ProbeRifle.GetCurrentState()) || !m_ProbeRifle.IsChamberFiredOut(0))
        {
            Report("FAIL: expected native WeaponFire with a spent casing");
            return false;
        }
        WeaponEventMechanism cycle = new WeaponEventMechanism(m_ProbeActor, null);
        if (m_ProbeRifle.ProcessWeaponEvent(cycle))
        {
            Report("FAIL: Bolt cycle accepted before the firing state finished");
            return false;
        }
        m_ProbeInventory.PostWeaponEvent(cycle);
        m_ProbeInventory.m_DeferredWeaponTimer.Run(timeout, m_ProbeInventory, "DeferredWeaponFailed");
        m_ProbeRounds = m_ProbeRifle.GetMagazine(0).GetAmmoCount();
        return true;
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_ProbeStage == 99 || !m_PredateSession.m_HuntReady || !m_PredateSession.m_PeerRoster) return;
        float now = GetGame().GetTickTime();
        PlayerBase player = PredateOperatorDeaths.HuntPlayer();
        if (m_ProbeStage == 0)
        {
            if (!m_PredateSession.m_ActorsActive || !player || !player.IsAlive() || !player.IsPlayerLoaded()) return;
            PredatePeerBinding peer = m_PredateSession.m_PeerRoster.Find("lynx-01");
            if (!peer || !peer.Actor) return;
            m_ProbeActor = peer.Actor;
            m_ProbeActor.SetAllowDamage(false);
            player.SetAllowDamage(false);
            m_ProbeActor.SetPosition(m_PredateSession.PredateGround(player.GetPosition() + "25 0 0"));
            m_ProbeActor.SetDirection("-1 0 0");
            EntityAI held = m_ProbeActor.GetHumanInventory().GetEntityInHands();
            if (held) GetGame().ObjectDelete(held);
            m_ProbeRifle = Weapon_Base.Cast(m_ProbeActor.GetHumanInventory().CreateInHands("BC_CZTSR_Black"));
            if (!m_ProbeRifle || !m_ProbeRifle.SpawnAmmo("BC_CZTSR_Magazine", WeaponWithAmmoFlags.CHAMBER | WeaponWithAmmoFlags.MAX_CAPACITY_MAG))
            {
                Report("FAIL: couldn't load the same CZ-TSR as game2");
                return;
            }
            m_ProbeInventory = m_ProbeActor.GetDayZPlayerInventory();
            m_ProbeAt = now + 1;
            m_ProbeStage = 1;
        }
        else if (m_ProbeStage == 1 && now >= m_ProbeAt)
        {
            if (!player || !m_ProbeActor.GetCommandModifier_Weapons() || !m_ProbeRifle.IsIdle()) return;
            if (!QueueCycle(player, 3)) return;
            player.SetAllowDamage(true);
            player.SetHealth("", "Health", 0);
            m_ProbeStage = 2;
            Report("Player death queued with a real bolt cycle pending");
        }
        else if (m_ProbeStage == 2 && m_ProbeActor.GetIsSimulationDisabled())
        {
            if (!m_ProbeInventory.m_DeferredWeaponEvent || m_ProbeInventory.m_DeferredWeaponTimer.IsRunning())
            {
                Report("FAIL: freezing the peer did not preserve and pause its pending weapon action");
                return;
            }
            m_ProbeRemaining = m_ProbeInventory.m_DeferredWeaponTimer.GetRemaining();
            m_ProbeAt = now + 7;
            m_ProbeStage = 3;
        }
        else if (m_ProbeStage == 3 && now >= m_ProbeAt)
        {
            float remaining = m_ProbeInventory.m_DeferredWeaponTimer.GetRemaining();
            if (!m_ProbeActor.GetIsSimulationDisabled() || !m_ProbeInventory.m_DeferredWeaponEvent || Math.AbsFloat(remaining - m_ProbeRemaining) > 0.01)
            {
                Report("FAIL: deferred action changed while the peer was frozen at the death screen");
                return;
            }
            if (m_ProbeRifle.GetMagazine(0).GetAmmoCount() != m_ProbeRounds)
            {
                Report("FAIL: ammunition changed while the operator was frozen");
                return;
            }
            Report("Death screen survived seven seconds with a pending bolt cycle");
            FileHandle ready = OpenFile("$profile:WeaponHoldRedeploy.txt", FileMode.WRITE);
            if (ready) CloseFile(ready);
            m_ProbeStage = 4;
        }
        else if (m_ProbeStage == 4 && m_PredateSession.m_WorldState.PlayerLife == 2 && !m_ProbeActor.GetIsSimulationDisabled())
        {
            m_ProbeAt = now + 4;
            m_ProbeStage = 5;
            Report("Redeployment resumed AI and its weapon timer");
        }
        else if (m_ProbeStage == 5 && now >= m_ProbeAt)
        {
            if (!player || m_ProbeInventory.m_DeferredWeaponEvent || m_ProbeInventory.m_DeferredWeaponTimer.IsRunning() || !m_ProbeRifle.IsIdle())
            {
                Report("FAIL: the deferred action did not settle after redeployment");
                return;
            }
            if (!m_ProbeRifle.IsChamberFull(0) || m_ProbeRifle.IsChamberFiredOut(0))
            {
                Report("FAIL: the resumed AI did not chamber its next round");
                return;
            }
            // Force a timeout before the firing state can finish to exercise
            // the recovery path during active play as well as the hold path.
            if (!QueueCycle(player, 0.01)) return;
            m_ProbeAt = now + 0.2;
            m_ProbeStage = 6;
        }
        else if (m_ProbeStage == 6 && now >= m_ProbeAt)
        {
            if (m_ProbeInventory.m_DeferredWeaponEvent || !m_ProbeRifle.IsIdle())
            {
                Report("FAIL: the active bolt timeout did not return the weapon to an idle state");
                return;
            }
            if (m_ProbeRifle.GetMagazine(0).GetAmmoCount() > m_ProbeRounds)
            {
                Report("FAIL: timeout recovery added ammunition");
                return;
            }
            Report("PASS");
        }
    }
}
