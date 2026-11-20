modded class DetachOldMagazine
{
    override void OnExit(WeaponEventBase e)
    {
        if (GetGame().IsClient() && e && eAIBase.Cast(e.m_player) && m_oldMagazine)
        {
            // Replication can move or unload a remote AI's magazine before
            // the animation finishes. Only move it while it is in that AI's hand.
            InventoryLocation location = new InventoryLocation;
            bool hasLocation = m_oldMagazine.GetInventory().GetCurrentInventoryLocation(location) && location.IsValid();
            bool stillHeld = hasLocation && location.GetType() == InventoryLocationType.ATTACHMENT && location.GetParent() == e.m_player && location.GetSlot() == InventorySlots.LEFTHAND;
            if (!stillHeld)
            {
                Print("[Predate] Skipped a stale remote AI magazine detachment.");
                m_oldMagazine = null;
                m_newDst = null;
            }
        }
        super.OnExit(e);
    }
}
