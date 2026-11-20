modded class eAIBase
{
    ItemBase m_PredateTreatment;
    float m_PredateTreatmentEnds;
    float m_PredateNextMedicalCheck;

    override bool PredateOwnActionPriority()
    {
        return m_PredateTreatment || super.PredateOwnActionPriority();
    }

    override bool eAI_CanFire(Weapon_Base weapon)
    {
        return !m_PredateTreatment && super.eAI_CanFire(weapon);
    }

    override bool eAI_CanLootWeapon(ItemBase weapon, eAIFaction faction)
    {
        if (m_PredateCombatEnabled && weapon)
        {
            string type = weapon.GetType();
            if (type != "M4A1" && type != "AK74" && type != "AKM" && type != "FAL" && type != "M14" && type != "SV98") return false;
        }
        return super.eAI_CanLootWeapon(weapon, faction);
    }

    bool PredateCanTreat()
    {
        if (!IsAlive() || IsUnconscious() || IsRestrained() || IsInTransport() || IsSwimming() || IsClimbing() || IsFighting()) return false;
        if (GetActionManager().GetRunningAction() || GetWeaponManager().IsRunning()) return false;
        if (GetGame().GetTickTime() - m_eAI_LastHitTime < 5) return false;
        eAITarget target = GetTarget();
        return !target || !target.IsPlayer() || !eAI_HasLOS();
    }

    bool PredateNeedsMedicine(string type, float now)
    {
        ModifiersManager modifiers = GetModifiersManager();
        if (!modifiers) return false;
        // Let Expansion's native bandaging state deal with cuts first.
        if (IsBleeding() && GetBandageToUse()) return false;
        if (type == "SalineBagIV")
            return !IsBleeding() && GetHealth01("", "Blood") < 0.72 && !modifiers.IsModifierActive(eModifiers.MDF_SALINE);
        if (type == "Morphine")
            return (GetBrokenLegs() == eBrokenLegs.BROKEN_LEGS || GetHealth01("", "Health") < 0.60) && !modifiers.IsModifierActive(eModifiers.MDF_MORPHINE);
        return false;
    }

    void PredateUpdateMedicine(float now)
    {
        if (m_PredateTreatment)
        {
            ItemBase medicine = m_PredateTreatment;
            if (medicine.GetHierarchyRootPlayer() != this || medicine.IsRuined() || !PredateCanTreat() || !PredateNeedsMedicine(medicine.GetType(), now))
            {
                m_PredateTreatment = null;
                m_PredateNextMedicalCheck = now + 2;
                return;
            }
            if (now < m_PredateTreatmentEnds) return;
            m_PredateTreatment = null;
            if (medicine.GetType() == "SalineBagIV")
            {
                medicine.TransferModifiers(this);
                GetModifiersManager().ActivateModifier(eModifiers.MDF_SALINE);
            }
            else medicine.OnApply(this);
            medicine.Delete();
            m_PredateNextMedicalCheck = now + 3;
            return;
        }
        if (now < m_PredateNextMedicalCheck || !PredateCanTreat()) return;
        m_PredateNextMedicalCheck = now + 1;
        bool saline = PredateNeedsMedicine("SalineBagIV", now);
        bool morphine = PredateNeedsMedicine("Morphine", now);
        if (!saline && !morphine) return;
        array<EntityAI> inventory = {};
        GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        ItemBase fluids;
        ItemBase painRelief;
        foreach (EntityAI entity : inventory)
        {
            ItemBase available = ItemBase.Cast(entity);
            if (!available || available.IsRuined()) continue;
            string itemType = available.GetType();
            if (saline && itemType == "SalineBagIV") fluids = available;
            else if (morphine && itemType == "Morphine") painRelief = available;
        }
        if (fluids) m_PredateTreatment = fluids;
        else m_PredateTreatment = painRelief;
        if (!m_PredateTreatment) return;
        m_PredateTreatmentEnds = now + 2.5;
        if (m_PredateTreatment == fluids) m_PredateTreatmentEnds = now + UATimeSpent.SALINE;
    }
}
