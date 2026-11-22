class PredateStartingKit
{
    string Id;
    string Name;
    string Description;
    string FirearmClass;
    string MagazineClass;
    int SpareMagazines;
    string SuppressorClass;
    string NightVisionClass;
    ref array<string> Attachments = new array<string>;
    ref array<string> WorkshopItems = new array<string>;

    bool Mounted()
    {
        if (!WorkshopItems || WorkshopItems.Count() == 0) return true;
        return PredateLoadoutCatalog.ClassExists(FirearmClass) && PredateLoadoutCatalog.ClassExists(NightVisionClass);
    }
}

class PredateModernKitEquipment
{
    string NightVisionClass;
    string ModuleClass;
    int SpareMagazines;
    ref array<string> WorkshopItems = {};
}

class PredateModernKitCatalog
{
    ref PredateModernKitEquipment Equipment;
    ref array<ref PredateStartingKit> Kits = {};
}

class PredateStartingSidearm
{
    string FirearmClass;
    string MagazineClass;
    string WorkshopFirearmClass;
    string WorkshopMagazineClass;
    string SuppressorClass;
    string LightClass;
    string HolsterClass;
    string MagazinePouchClass;
    int SpareMagazines;

    bool HasWorkshopSidearm()
    {
        if (WorkshopFirearmClass == "" || WorkshopMagazineClass == "") return false;
        return PredateLoadoutCatalog.ClassExists(WorkshopFirearmClass) && PredateLoadoutCatalog.ClassExists(WorkshopMagazineClass);
    }

    string IssuedMagazineClass()
    {
        if (HasWorkshopSidearm()) return WorkshopMagazineClass;
        return MagazineClass;
    }
}

class PredateOutfit
{
    string Id;
    string Name;
    string Body;
    string Legs;
    string Feet;
    string Mask;
    string Vest;
    string Pouches;
    string Gloves;
    string Headgear;
    string Eyewear;
    string NightVision;
    string WorkshopNightVision;
    bool InRotation;
    bool Legacy;
}

class PredateLoadoutCatalog
{
    int Schema;
    int StartingBandages;
    int StartingMorphine;
    string StartingRangefinder;
    ref array<ref PredateOutfit> Outfits = new array<ref PredateOutfit>;
    string DefaultOutfit;
    string NightVisionMount;
    ref PredateStartingSidearm Sidearm;
    ref array<ref PredateStartingKit> Kits = new array<ref PredateStartingKit>;
    ref array<ref PredateStartingKit> OperatorKits = new array<ref PredateStartingKit>;
    ref array<ref PredateStartingKit> ModernOperatorKits = {};
    string ValidationError;

    bool Invalid(string message)
    {
        ValidationError = message;
        return false;
    }

    PredateStartingKit Find(string id)
    {
        if (!Kits) return null;
        foreach (PredateStartingKit kit : Kits)
            if (kit && kit.Id == id && kit.Mounted()) return kit;
        foreach (PredateStartingKit operatorKit : OperatorKits)
            if (operatorKit && operatorKit.Id == id && operatorKit.Mounted()) return operatorKit;
        foreach (PredateStartingKit modernKit : ModernOperatorKits)
            if (modernKit.Id == id) return modernKit;
        return null;
    }

    PredateStartingKit FindRifle(string firearm)
    {
        foreach (PredateStartingKit kit : Kits)
            if (kit.FirearmClass == firearm && kit.Mounted()) return kit;
        foreach (PredateStartingKit operatorKit : OperatorKits)
            if (operatorKit.FirearmClass == firearm && operatorKit.Mounted()) return operatorKit;
        foreach (PredateStartingKit modernKit : ModernOperatorKits)
            if (modernKit.FirearmClass == firearm) return modernKit;
        return null;
    }

    bool LoadModernOperatorKits(bool required)
    {
        PredateModernKitCatalog modern;
        string error;
        if (!JsonFileLoader<PredateModernKitCatalog>.LoadFile("Predate/data/modern-loadouts.json", modern, error) || !modern)
            return Invalid("Modern starting-kit catalog could not be loaded: " + error);
        PredateModernKitEquipment equipment = modern.Equipment;
        if (!equipment || !equipment.WorkshopItems || equipment.WorkshopItems.Count() == 0 || equipment.NightVisionClass == "" || equipment.ModuleClass == "" || !modern.Kits || modern.Kits.Count() == 0)
            return Invalid("Modern starting-kit catalog is incomplete");
        foreach (PredateStartingKit kit : modern.Kits)
        {
            if (!kit || !kit.Attachments) return Invalid("Invalid modern starting kit");
            kit.WorkshopItems = equipment.WorkshopItems;
            kit.NightVisionClass = equipment.NightVisionClass;
            kit.SpareMagazines = equipment.SpareMagazines;
            if (!kit.Mounted())
            {
                if (required) return Invalid("Missing Workshop equipment for " + kit.Name + ". Mount Buca Weapons and Forward Operator Gear.");
                continue;
            }
            if (kit.Attachments.Find(equipment.ModuleClass) < 0) kit.Attachments.Insert(equipment.ModuleClass);
            ModernOperatorKits.Insert(kit);
        }
        return true;
    }

    static bool ClassExists(string name)
    {
        return GetGame().ConfigIsExisting("CfgWeapons " + name) || GetGame().ConfigIsExisting("CfgVehicles " + name) || GetGame().ConfigIsExisting("CfgMagazines " + name);
    }

    static string IssuedAttachment(string name)
    {
        if (name == "PredateWeaponModule" && GetGame().ConfigIsExisting("CfgPatches Predate_MutatedLasers"))
            return "MM_ANPEQ15";
        return name;
    }

    PredateOutfit FindOutfit(string id)
    {
        if (id == "") id = DefaultOutfit;
        foreach (PredateOutfit outfit : Outfits)
            if (outfit && outfit.Id == id) return outfit;
        return null;
    }

    PredateOutfit RotatingOutfit(int offset)
    {
        array<PredateOutfit> choices = {};
        foreach (PredateOutfit outfit : Outfits)
            if (outfit.InRotation && !outfit.Legacy) choices.Insert(outfit);
        return choices[offset % choices.Count()];
    }

    bool Valid()
    {
        if (Schema != 1 || !Kits || Kits.Count() < 1 || Kits.Count() > 16) return Invalid("Invalid schema or kit count");
        if (StartingBandages < 1 || StartingBandages > 12) return Invalid("Invalid starting bandage count");
        if (StartingMorphine < 1 || StartingMorphine > 12) return Invalid("Invalid starting morphine count");
        if (StartingRangefinder == "" || !ClassExists(StartingRangefinder)) return Invalid("Missing starting rangefinder");
        if (!OperatorKits || OperatorKits.Count() < 1 || OperatorKits.Count() > 16) return Invalid("Invalid operator kit count");
        if (!Outfits || !FindOutfit(DefaultOutfit) || !ClassExists(NightVisionMount)) return Invalid("Missing starting outfits or night vision mount");
        if (!Sidearm || Sidearm.SpareMagazines < 1 || Sidearm.SpareMagazines > 12) return Invalid("Invalid starting sidearm");
        array<string> sidearmEquipment = {Sidearm.FirearmClass, Sidearm.MagazineClass, Sidearm.SuppressorClass, Sidearm.LightClass, Sidearm.HolsterClass, Sidearm.MagazinePouchClass};
        foreach (string sidearmItem : sidearmEquipment)
            if (sidearmItem == "" || !ClassExists(sidearmItem)) return Invalid("Missing starting sidearm item: " + sidearmItem);
        array<string> outfitIds = {};
        bool hasRotation;
        foreach (PredateOutfit outfit : Outfits)
        {
            if (!outfit || outfit.Id == "" || outfit.Id == "varied" || outfit.Name == "" || outfitIds.Find(outfit.Id) >= 0) return Invalid("Invalid outfit ID");
            if (!ClassExists(outfit.Body) || !ClassExists(outfit.Legs) || !ClassExists(outfit.Feet)) return Invalid("Invalid starting outfit: " + outfit.Id);
            array<string> equipment = {outfit.Mask, outfit.Vest, outfit.Pouches, outfit.Gloves, outfit.Headgear, outfit.Eyewear, outfit.NightVision};
            foreach (string item : equipment)
                if (item != "" && !ClassExists(item)) return Invalid("Missing outfit equipment: " + item);
            outfitIds.Insert(outfit.Id);
            if (outfit.InRotation && !outfit.Legacy) hasRotation = true;
        }
        if (!hasRotation) return Invalid("No outfits in the AI rotation");
        array<PredateStartingKit> combined = {};
        foreach (PredateStartingKit playerKit : Kits) combined.Insert(playerKit);
        foreach (PredateStartingKit operatorKit : OperatorKits) combined.Insert(operatorKit);
        foreach (PredateStartingKit modernKit : ModernOperatorKits) combined.Insert(modernKit);
        array<string> ids = new array<string>;
        foreach (PredateStartingKit kit : combined)
        {
            if (!kit || kit.Id == "" || ids.Find(kit.Id) >= 0 || kit.Name == "" || kit.Description == "") return Invalid("Invalid kit identity or description");
            ids.Insert(kit.Id);
            if (!kit.Mounted()) continue;
            if (!ClassExists(kit.FirearmClass)) return Invalid("Missing firearm: " + kit.FirearmClass);
            if (!ClassExists(kit.MagazineClass)) return Invalid("Missing magazine: " + kit.MagazineClass);
            if (!ClassExists(kit.SuppressorClass)) return Invalid("Missing rifle suppressor: " + kit.SuppressorClass);
            if (kit.NightVisionClass != "" && !GetGame().ConfigIsExisting("CfgVehicles " + kit.NightVisionClass))
                return Invalid("Missing night vision: " + kit.NightVisionClass);
            if (kit.SpareMagazines < 0 || kit.SpareMagazines > 12 || !kit.Attachments) return Invalid("Invalid ammunition or attachments: " + kit.Id);
            foreach (string attachment : kit.Attachments)
                if (!ClassExists(attachment)) return Invalid("Missing attachment: " + attachment);
        }
        if (!Find("m4a1")) return Invalid("Default kit missing");
        return true;
    }
}
