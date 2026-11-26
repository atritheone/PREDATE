class PredateProjectileArming
{
    static bool CanTrack(Weapon_Base weapon)
    {
        if (!PredateProjectileRounds.Enabled || !GetGame().IsServer()) return false;
        Man shooter = weapon.GetHierarchyRootPlayer();
        return shooter && shooter.GetIdentity();
    }

    static PredateProjectileRound Arm(Weapon_Base weapon, int muzzle)
    {
        if (weapon.IsChamberEmpty(muzzle) || weapon.IsChamberFiredOut(muzzle)) return null;
        string ammo;
        float cartridgeDamage;
        if (!weapon.GetCartridgeInfo(muzzle, cartridgeDamage, ammo) || ammo.IndexOf(PredateProjectileRounds.PREFIX) == 0) return null;
        float lifetime;
        if (!PredateProjectileRounds.Lifetimes.Find(ammo, lifetime))
        {
            if (GetGame().ConfigIsExisting("CfgAmmo " + PredateProjectileRounds.PREFIX + "0_" + ammo))
                lifetime = GetGame().ConfigGetFloat("CfgAmmo " + ammo + " timeToLive");
            PredateProjectileRounds.Lifetimes.Set(ammo, lifetime);
        }
        if (lifetime <= 0) return null;

        for (int attempt = 0; attempt < PredateProjectileRounds.SLOT_COUNT; attempt++)
        {
            int slot = PredateProjectileRounds.NextSlot;
            PredateProjectileRounds.NextSlot = (slot + 1) % PredateProjectileRounds.SLOT_COUNT;
            string tag = PredateProjectileRounds.PREFIX + slot + "_" + ammo;
            if (PredateProjectileRounds.Active.Contains(tag)) continue;
            if (!weapon.PopCartridgeFromChamber(muzzle, cartridgeDamage, ammo)) return null;
            if (!weapon.PushCartridgeToChamber(muzzle, cartridgeDamage, tag))
            {
                weapon.PushCartridgeToChamber(muzzle, cartridgeDamage, ammo);
                return null;
            }
            PredateProjectileRound round = new PredateProjectileRound;
            round.Tag = tag;
            round.BaseAmmo = ammo;
            round.Weapon = weapon;
            round.Muzzle = muzzle;
            round.CartridgeDamage = cartridgeDamage;
            round.Lifetime = lifetime;
            vector muzzlePosition = weapon.GetSelectionPositionMS("usti hlavne");
            if (muzzlePosition != vector.Zero) round.Origin = weapon.ModelToWorld(muzzlePosition);
            else round.Origin = weapon.GetHierarchyRootPlayer().GetPosition();
            PredateProjectileRounds.Active.Set(tag, round);
            if (!weapon.m_PredateArmedAmmo) weapon.m_PredateArmedAmmo = new map<int, string>;
            weapon.m_PredateArmedAmmo.Set(muzzle, tag);
            return round;
        }
        return null;
    }

    static void RestoreUnfired(Weapon_Base weapon, PredateProjectileRound round)
    {
        if (!round) return;
        weapon.m_PredateArmedAmmo.Remove(round.Muzzle);
        if (round.Fired)
        {
            if (round.Number == 0) PredateProjectileRounds.RejectedLaunch.Invoke(round);
            return;
        }
        weapon.m_PredateFiredAmmo = "";
        if (weapon.IsChamberFiredOut(round.Muzzle))
        {
            // Preserve a rejected cartridge's casing state and quarantine
            // its tag, in case the engine still emits a projectile event.
            round.Closed = true;
            PredateProjectileRounds.RejectedLaunch.Invoke(round);
            return;
        }
        string ammo;
        float damage;
        if (weapon.GetCartridgeInfo(round.Muzzle, damage, ammo) && ammo == round.Tag)
        {
            if (!weapon.PopCartridgeFromChamber(round.Muzzle, damage, ammo)) return;
            if (!weapon.PushCartridgeToChamber(round.Muzzle, round.CartridgeDamage, round.BaseAmmo)) return;
        }
        PredateProjectileRounds.Active.Remove(round.Tag);
    }
}

modded class WeaponFire
{
    override void OnEntry(WeaponEventBase e)
    {
        PredateProjectileRound round;
        if (e && PredateProjectileArming.CanTrack(m_weapon))
            round = PredateProjectileArming.Arm(m_weapon, m_weapon.GetCurrentMuzzle());
        super.OnEntry(e);
        PredateProjectileArming.RestoreUnfired(m_weapon, round);
    }
}

modded class WeaponFireWithEject
{
    override void OnEntry(WeaponEventBase e)
    {
        PredateProjectileRound round;
        if (e && PredateProjectileArming.CanTrack(m_weapon))
            round = PredateProjectileArming.Arm(m_weapon, m_weapon.GetCurrentMuzzle());
        super.OnEntry(e);
        PredateProjectileArming.RestoreUnfired(m_weapon, round);
    }
}

modded class WeaponFireToJam
{
    override void OnEntry(WeaponEventBase e)
    {
        PredateProjectileRound round;
        if (e && PredateProjectileArming.CanTrack(m_weapon))
            round = PredateProjectileArming.Arm(m_weapon, m_weapon.GetCurrentMuzzle());
        super.OnEntry(e);
        PredateProjectileArming.RestoreUnfired(m_weapon, round);
    }
}

modded class WeaponFireMultiMuzzle
{
    override void OnEntry(WeaponEventBase e)
    {
        if (!e || !PredateProjectileArming.CanTrack(m_weapon))
        {
            super.OnEntry(e);
            return;
        }
        array<ref PredateProjectileRound> rounds = new array<ref PredateProjectileRound>;
        int muzzle = m_weapon.GetCurrentMuzzle();
        int burst = m_weapon.GetCurrentModeBurstSize(muzzle);
        if (burst > 1)
        {
            for (int i = 0; i < burst; i++)
                rounds.Insert(PredateProjectileArming.Arm(m_weapon, i));
        }
        else rounds.Insert(PredateProjectileArming.Arm(m_weapon, muzzle));
        super.OnEntry(e);
        foreach (PredateProjectileRound round : rounds)
            PredateProjectileArming.RestoreUnfired(m_weapon, round);
    }
}
