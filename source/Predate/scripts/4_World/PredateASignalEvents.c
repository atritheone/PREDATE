// Count server-confirmed firearm events. UI requests cannot manufacture activity.
modded class PlayerBase
{
    bool m_PredateSignatureRegistered;
    int m_PredatePendingShots;
    int m_PredatePendingExplosions;
}

modded class Grenade_Base
{
    PlayerBase m_PredateExplosionOwner;
    bool m_PredateExplosionCounted;

    void PredateRememberExplosionOwner(Man holder)
    {
        if (!GetGame().IsServer()) return;
        PlayerBase owner = PlayerBase.Cast(holder);
        if (owner && owner.m_PredateSignatureRegistered) m_PredateExplosionOwner = owner;
        else m_PredateExplosionOwner = null;
    }

    override void OnInventoryEnter(Man player)
    {
        super.OnInventoryEnter(player);
        PredateRememberExplosionOwner(player);
    }

    override void OnInventoryExit(Man player)
    {
        super.OnInventoryExit(player);
        PredateRememberExplosionOwner(player);
    }

    override protected void OnUnpin()
    {
        PredateRememberExplosionOwner(GetHierarchyRootPlayer());
        super.OnUnpin();
    }

    override protected void InitiateExplosion()
    {
        EGrenadeType kind = GetGrenadeType();
        bool physicalBlast = kind == EGrenadeType.FRAGMENTATION || kind == EGrenadeType.ILLUMINATING;
        if (GetGame().IsServer() && !m_PredateExplosionCounted && physicalBlast && m_AmmoTypes && m_AmmoTypes.Count() > 0)
        {
            m_PredateExplosionCounted = true;
            PlayerBase owner = m_PredateExplosionOwner;
            // Credit only recorded ownership. Nearby people do not inherit an
            // unknown blast, and the receiver still waits for its next pulse.
            if (owner && owner.m_PredateSignatureRegistered)
                owner.m_PredatePendingExplosions = Math.Min(8, owner.m_PredatePendingExplosions + 1);
        }
        super.InitiateExplosion();
    }
}

modded class Weapon_Base
{
    override void EEFired(int muzzleType, int mode, string ammoType)
    {
        super.EEFired(muzzleType, mode, ammoType);
        if (!GetGame().IsServer()) return;
        PlayerBase owner = PlayerBase.Cast(GetHierarchyRootPlayer());
        if (owner && owner.m_PredateSignatureRegistered)
            owner.m_PredatePendingShots = Math.Min(32, owner.m_PredatePendingShots + 1);
    }
}
