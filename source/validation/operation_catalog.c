class PredateCatalogItem
{
    string id;
    string name;
    string kind;
    int width;
    int height;
    int cargoWidth;
    int cargoHeight;
    int capacity;
    ref array<string> slots = new array<string>;
    ref array<string> accepts = new array<string>;
    ref array<string> magazines = new array<string>;
}

class PredateMapPlace
{
    string name;
    string kind;
    float x;
    float z;
}

class PredateSetupExport
{
    int size;
    ref array<ref PredateCatalogItem> items = new array<ref PredateCatalogItem>;
    ref array<ref PredateMapPlace> places = new array<ref PredateMapPlace>;
}

class PredateExportMission : MissionServer
{
    int m_Row;
    FileHandle m_HeightFile;
    const int GRID = 512;

    void AddCatalog(PredateSetupExport data, string root, string kind)
    {
        for (int i = 0; i < GetGame().ConfigGetChildrenCount(root); i++)
        {
            string id;
            GetGame().ConfigGetChildName(root, i, id);
            string path = root + " " + id;
            if (GetGame().ConfigGetInt(path + " scope") != 2) continue;
            array<int> dimensions = new array<int>;
            if (GetGame().ConfigIsExisting(path + " itemSize")) GetGame().ConfigGetIntArray(path + " itemSize", dimensions);
            if (dimensions.Count() != 2 || dimensions[0] <= 0 || dimensions[1] <= 0) continue;
            if (kind == "item" && !GetGame().IsKindOf(id, "Inventory_Base")) continue;
            PredateCatalogItem item = new PredateCatalogItem;
            item.id = id;
            item.kind = kind;
            item.width = dimensions[0];
            item.height = dimensions[1];
            GetGame().ConfigGetText(path + " displayName", item.name);
            if (item.name == "") item.name = id;
            if (GetGame().ConfigIsExisting(path + " inventorySlot")) GetGame().ConfigGetTextArray(path + " inventorySlot", item.slots);
            if (GetGame().ConfigIsExisting(path + " attachments")) GetGame().ConfigGetTextArray(path + " attachments", item.accepts);
            if (GetGame().ConfigIsExisting(path + " magazines")) GetGame().ConfigGetTextArray(path + " magazines", item.magazines);
            array<int> cargo = new array<int>;
            if (GetGame().ConfigIsExisting(path + " itemsCargoSize")) GetGame().ConfigGetIntArray(path + " itemsCargoSize", cargo);
            if (cargo.Count() == 2)
            {
                item.cargoWidth = cargo[0];
                item.cargoHeight = cargo[1];
            }
            if (kind == "magazine")
                item.capacity = GetGame().ConfigGetInt(path + " count");
            else if (GetGame().ConfigIsExisting(path + " varQuantityMax"))
                item.capacity = GetGame().ConfigGetInt(path + " varQuantityMax");
            data.items.Insert(item);
        }
    }

    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExportCatalog, 1000, false);
    }

    void ExportCatalog()
    {
        PredateSetupExport data = new PredateSetupExport;
        data.size = GetGame().GetWorld().GetWorldSize();
        AddCatalog(data, "CfgWeapons", "weapon");
        AddCatalog(data, "CfgMagazines", "magazine");
        AddCatalog(data, "CfgVehicles", "item");
        string root = "CfgWorlds " + GetGame().GetWorldName() + " Names";
        for (int i = 0; i < GetGame().ConfigGetChildrenCount(root); i++)
        {
            string child;
            GetGame().ConfigGetChildName(root, i, child);
            string path = root + " " + child;
            array<float> pos = new array<float>;
            GetGame().ConfigGetFloatArray(path + " position", pos);
            if (pos.Count() < 2) continue;
            PredateMapPlace place = new PredateMapPlace;
            place.x = pos[0];
            place.z = pos[1];
            GetGame().ConfigGetText(path + " name", place.name);
            GetGame().ConfigGetText(path + " type", place.kind);
            data.places.Insert(place);
        }
        string error;
        if (!JsonFileLoader<PredateSetupExport>.SaveFile("$profile:SetupExport.json", data, error))
            Print(error);
        m_HeightFile = OpenFile("$profile:TerrainExport.csv", FileMode.WRITE);
        if (m_HeightFile) GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExportTerrain, 10, true);
    }

    void ExportTerrain()
    {
        float metres = GetGame().GetWorld().GetWorldSize() / (GRID * 1.0);
        for (int n = 0; n < 8 && m_Row < GRID; n++)
        {
            float z = (GRID - m_Row - 0.5) * metres;
            string row = "";
            for (int col = 0; col < GRID; col++)
            {
                float x = (col + 0.5) * metres;
                int height = Math.Round(GetGame().SurfaceY(x,z));
                if (GetGame().SurfaceIsSea(x,z) || GetGame().SurfaceIsPond(x,z)) height = -9999;
                if (col > 0) row += ",";
                row += height.ToString();
            }
            FPrintln(m_HeightFile, row);
            m_Row++;
        }
        if (m_Row == GRID)
        {
            CloseFile(m_HeightFile);
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ExportTerrain);
            Print("[Predate] Setup export complete");
        }
    }
}

void main() {}

Mission CreateCustomMission(string path)
{
    return new PredateExportMission;
}
