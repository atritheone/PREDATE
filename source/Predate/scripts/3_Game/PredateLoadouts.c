class PredateStartingKit
{
    string Id;
    string Name;
    string Description;
    string FirearmClass;
    string MagazineClass;
    int SpareMagazines;
    string SuppressorClass;
    ref array<string> Attachments = new array<string>;
}

class PredateLoadoutCatalog
{
    int Schema;
    ref array<ref PredateStartingKit> Kits = new array<ref PredateStartingKit>;
    ref array<ref PredateStartingKit> OperatorKits = new array<ref PredateStartingKit>;
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
            if (kit && kit.Id == id) return kit;
        foreach (PredateStartingKit operatorKit : OperatorKits)
            if (operatorKit && operatorKit.Id == id) return operatorKit;
        return null;
    }

    PredateStartingKit FindRifle(string firearm)
    {
        foreach (PredateStartingKit kit : Kits)
            if (kit.FirearmClass == firearm) return kit;
        foreach (PredateStartingKit operatorKit : OperatorKits)
            if (operatorKit.FirearmClass == firearm) return operatorKit;
        return null;
    }

    static bool ClassExists(string name)
    {
        return GetGame().ConfigIsExisting("CfgWeapons " + name) || GetGame().ConfigIsExisting("CfgVehicles " + name) || GetGame().ConfigIsExisting("CfgMagazines " + name);
    }

    bool Valid()
    {
        if (Schema != 1 || !Kits || Kits.Count() < 1 || Kits.Count() > 16) return Invalid("Invalid schema or kit count");
        if (!OperatorKits || OperatorKits.Count() < 1 || OperatorKits.Count() > 16) return Invalid("Invalid operator kit count");
        array<PredateStartingKit> combined = {};
        foreach (PredateStartingKit playerKit : Kits) combined.Insert(playerKit);
        foreach (PredateStartingKit operatorKit : OperatorKits) combined.Insert(operatorKit);
        array<string> ids = new array<string>;
        foreach (PredateStartingKit kit : combined)
        {
            if (!kit || kit.Id == "" || ids.Find(kit.Id) >= 0 || kit.Name == "" || kit.Description == "") return Invalid("Invalid kit identity or description");
            if (!ClassExists(kit.FirearmClass)) return Invalid("Missing firearm: " + kit.FirearmClass);
            if (!ClassExists(kit.MagazineClass)) return Invalid("Missing magazine: " + kit.MagazineClass);
            if (!ClassExists(kit.SuppressorClass)) return Invalid("Missing rifle suppressor: " + kit.SuppressorClass);
            if (kit.SpareMagazines < 0 || kit.SpareMagazines > 12 || !kit.Attachments) return Invalid("Invalid ammunition or attachments: " + kit.Id);
            foreach (string attachment : kit.Attachments)
                if (!ClassExists(attachment)) return Invalid("Missing attachment: " + attachment);
            ids.Insert(kit.Id);
        }
        if (!Find("m4a1")) return Invalid("Default kit missing");
        return true;
    }
}
