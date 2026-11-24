class PredateActorSimulation
{
    static void SetHeld(Man actor, bool held)
    {
        PlayerBase player = PlayerBase.Cast(actor);
        eAIBase operator = eAIBase.Cast(actor);
        if (operator && operator.m_PredateCombatEnabled)
            PredateAIWeaponTimer.Install(operator.GetDayZPlayerInventory());
        if (held && player) player.PredateHoldWeaponTimer();
        actor.DisableSimulation(held);
        if (!held && player) player.PredateResumeWeaponTimer();
    }
}

modded class PlayerBase
{
    WeaponEventBase m_PredateHeldWeaponEvent;

    void PredateHoldWeaponTimer()
    {
        DayZPlayerInventory inventory = GetDayZPlayerInventory();
        // Weapon commands stop with the actor but the system timer queue keeps running.
        if (!inventory || !inventory.m_DeferredWeaponEvent || !inventory.m_DeferredWeaponTimer.IsRunning()) return;
        m_PredateHeldWeaponEvent = inventory.m_DeferredWeaponEvent;
        inventory.m_DeferredWeaponTimer.Pause();
    }

    void PredateResumeWeaponTimer()
    {
        if (!m_PredateHeldWeaponEvent) return;
        DayZPlayerInventory inventory = GetDayZPlayerInventory();
        if (inventory && inventory.m_DeferredWeaponEvent == m_PredateHeldWeaponEvent)
            inventory.m_DeferredWeaponTimer.Continue();
        m_PredateHeldWeaponEvent = null;
    }
}

// DayZPlayerInventory is engine-owned. Use its timer slot so that unrelated
// failures still follow the original engine callback.
class PredateAIWeaponTimer : Timer
{
    static void Install(DayZPlayerInventory inventory)
    {
        if (!inventory || PredateAIWeaponTimer.Cast(inventory.m_DeferredWeaponTimer)) return;
        Timer previous = inventory.m_DeferredWeaponTimer;
        float remaining;
        if (previous && previous.IsRunning())
        {
            if (inventory.m_DeferredWeaponEvent)
                remaining = Math.Max(0.001, previous.GetRemaining());
            previous.Stop();
        }
        inventory.m_DeferredWeaponTimer = new PredateAIWeaponTimer;
        if (remaining > 0)
            inventory.m_DeferredWeaponTimer.Run(remaining, inventory, "DeferredWeaponFailed");
    }

    override protected void OnTimer()
    {
        DayZPlayerInventory inventory = DayZPlayerInventory.Cast(m_target);
        if (m_function == "DeferredWeaponFailed" && inventory && RecoverBoltCycle(inventory)) return;
        super.OnTimer();
    }

    protected bool RecoverBoltCycle(DayZPlayerInventory inventory)
    {
        WeaponEventBase pending = inventory.m_DeferredWeaponEvent;
        if (!pending) return true;
        if (pending.GetEventID() != WeaponEventID.MECHANISM) return false;
        eAIBase actor = eAIBase.Cast(inventory.GetDayZPlayerOwner());
        BoltActionRifle_Base rifle = BoltActionRifle_Base.Cast(inventory.GetEntityInHands());
        if (!actor || !actor.m_PredateCombatEnabled || !rifle || !WeaponFire.Cast(rifle.GetCurrentState())) return false;

        // Recover from the physical chamber and magazine state so the AI can
        // retry the cycle without adding or losing cartridges.
        inventory.CancelWeaponEvent();
        inventory.AbortWeaponEvent();
        if (!rifle.IsIdle()) rifle.RandomizeFSMState();
        actor.GetWeaponManager().OnWeaponActionEnd();
        rifle.Synchronize();
        if (PredateServerDiagnostics.Enabled)
            Print("[Predate] Recovered stalled AI bolt cycle on " + rifle.GetType());
        return true;
    }
}

modded class BoltActionRifle_Base
{
    override bool ProcessWeaponEvent(WeaponEventBase e)
    {
        if (GetGame().IsServer() && e.GetEventID() == WeaponEventID.MECHANISM)
        {
            eAIBase actor = eAIBase.Cast(e.m_player);
            // Expansion sends the event to replicas before processing it on
            // the server. Do not send a bolt cycle the firing state will reject.
            if (actor && actor.m_PredateCombatEnabled && WeaponFire.Cast(GetCurrentState())) return false;
        }
        return super.ProcessWeaponEvent(e);
    }
}
