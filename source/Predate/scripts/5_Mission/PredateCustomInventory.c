// The launcher supplies a parent-first inventory. DayZ resolves actual placement
// and compatibility against the mods attached to the operation.
class PredateCustomInventory
{
    static ref map<string, int> s_AttachmentSlots;

    static int SlotId(string name)
    {
        name.ToLower();
        if (!s_AttachmentSlots)
        {
            s_AttachmentSlots = new map<string, int>;
            int count = GetGame().ConfigGetChildrenCount("CfgSlots");
            for (int i = 0; i < count; i++)
            {
                string configClass;
                GetGame().ConfigGetChildName("CfgSlots", i, configClass);
                if (configClass.IndexOf("Slot_") != 0) continue;
                int id = InventorySlots.GetSlotIdFromString(configClass.Substring(5, configClass.Length() - 5));
                if (id == InventorySlots.INVALID) continue;
                string configuredName;
                GetGame().ConfigGetText("CfgSlots " + configClass + " name", configuredName);
                if (configuredName == "") continue;
                configuredName.ToLower();
                s_AttachmentSlots.Set(configuredName, id);
            }
        }
        int slot;
        if (s_AttachmentSlots.Find(name, slot)) return slot;
        return InventorySlots.INVALID;
    }

    static bool Error(out string message, int index, string detail)
    {
        message = "Starting inventory item " + (index + 1) + ": " + detail;
        return false;
    }

    static bool Validate(array<ref PredateInventoryNode> nodes, out string message)
    {
        if (!nodes || nodes.Count() > 256)
        {
            message = "The starting inventory must contain 0 to 256 items";
            return false;
        }
        bool hands;
        foreach (int index, PredateInventoryNode node : nodes)
        {
            if (!node || node.Parent < -1 || node.Parent >= index) return Error(message, index, "invalid inventory parent");
            string kind = node.ClassName;
            bool configured = kind != "" && (GetGame().ConfigIsExisting("CfgVehicles " + kind) || GetGame().ConfigIsExisting("CfgWeapons " + kind) || GetGame().ConfigIsExisting("CfgMagazines " + kind));
            if (!configured) return Error(message, index, "item class unavailable " + kind);
            if (!PredateInformationMath.Finite(node.Amount) || (node.Amount < 0 && node.Amount != -1)) return Error(message, index, "invalid amount for " + kind);
            if (node.Placement == "hands")
            {
                if (hands || node.Parent != -1) return Error(message, index, "hands are already occupied or not on the player");
                hands = true;
            }
            else if (node.Placement == "attachment")
            {
                if (SlotId(node.Slot) == InventorySlots.INVALID) return Error(message, index, "unknown attachment slot " + node.Slot);
            }
            else if (node.Placement == "magazine")
            {
                if (node.Parent < 0) return Error(message, index, "a loaded magazine needs a firearm parent");
            }
            else if (node.Placement == "cargo")
            {
                if (node.Parent < 0 || node.Row < -1 || node.Column < -1 || ((node.Row == -1) != (node.Column == -1)))
                    return Error(message, index, "cargo needs a container and valid row and column");
            }
            else return Error(message, index, "unknown placement " + node.Placement);
        }
        return true;
    }

    static EntityAI Create(PlayerBase player, EntityAI parent, PredateInventoryNode node)
    {
        if (node.Placement == "hands") return player.GetHumanInventory().CreateInHands(node.ClassName);
        GameInventory inventory = parent.GetInventory();
        if (!inventory) return null;
        if (node.Placement == "attachment") return inventory.CreateAttachmentEx(node.ClassName, SlotId(node.Slot));
        if (node.Placement == "magazine")
        {
            Weapon_Base weapon = Weapon_Base.Cast(parent);
            if (!weapon || weapon.GetMagazineTypeCount(0) == 0 || weapon.GetMagazine(0)) return null;
            int flags = WeaponWithAmmoFlags.NONE;
            if (node.Amount < 0) flags = WeaponWithAmmoFlags.CHAMBER;
            return weapon.SpawnAttachedMagazine(node.ClassName, flags);
        }
        CargoBase cargo = inventory.GetCargo();
        if (!cargo) return null;
        if (node.Row < 0) return inventory.CreateEntityInCargo(node.ClassName);
        return inventory.CreateEntityInCargoEx(node.ClassName, 0, node.Row, node.Column, node.Flipped);
    }

    static bool ApplyAmount(EntityAI entity, float amount)
    {
        if (amount < 0) return true;
        Magazine magazine = Magazine.Cast(entity);
        if (magazine)
        {
            if (amount > magazine.GetAmmoMax()) return false;
            int rounds = Math.Floor(amount);
            if (rounds != amount) return false;
            magazine.ServerSetAmmoCount(rounds);
            return magazine.GetAmmoCount() == rounds;
        }
        ItemBase item = ItemBase.Cast(entity);
        if (!item || !item.HasQuantity() || amount < item.GetQuantityMin() || amount > item.GetQuantityMax()) return false;
        item.SetQuantity(amount, false);
        return Math.AbsFloat(item.GetQuantity() - amount) < 0.001;
    }

    static bool Equip(PlayerBase player, array<ref PredateInventoryNode> nodes, out string message)
    {
        player.RemoveAllItems();
        array<EntityAI> created = {};
        foreach (int index, PredateInventoryNode node : nodes)
        {
            EntityAI parent = player;
            if (node.Parent >= 0) parent = created[node.Parent];
            EntityAI entity = Create(player, parent, node);
            if (!entity) return Error(message, index, node.ClassName + " does not fit at " + node.Placement + " " + node.Slot);
            if (!ApplyAmount(entity, node.Amount)) return Error(message, index, "amount cannot be applied to " + node.ClassName);
            created.Insert(entity);
        }
        return true;
    }
}
