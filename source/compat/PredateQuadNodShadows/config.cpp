class CfgPatches
{
    class Predate_QuadNodShadows
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"FOG_GPNVG_Tarzier_stuff", "FOG_GPNVG_Elite_stuff"};
    };
};

class CfgMods
{
    class PredateQuadNodShadows
    {
        dir = "PredateQuadNodShadows";
        type = "mod";
        dependencies[] = {"Mission"};
        class defs
        {
            class missionScriptModule
            {
                value = "";
                files[] = {"PredateQuadNodShadows/scripts/5_Mission"};
            };
        };
    };
};

class CfgVehicles
{
    class NVGoggles;
    class FOG_GPNVG_Base: NVGoggles
    {
        model = "\PredateQuadNodShadows\FOG_GPNVG.p3d";
    };
    class FOG_GPNVG_GSGM_Base: NVGoggles
    {
        model = "\PredateQuadNodShadows\FOG_GPNVG_GSGM.p3d";
    };
    class FOG_GPNVG_Elite: NVGoggles
    {
        model = "\PredateQuadNodShadows\GPNVG_Elite.p3d";
    };

    class Inventory_Base;
    class PredateQuadNodCaster: Inventory_Base
    {
        scope = 1;
        inventorySlot[] = {};
        model = "\PredateQuadNodShadows\FOG_GPNVG_Caster.p3d";
    };
    class PredateQuadNodGSGMCaster: PredateQuadNodCaster
    {
        model = "\PredateQuadNodShadows\FOG_GPNVG_GSGM_Caster.p3d";
    };
    class PredateQuadNodEliteCaster: PredateQuadNodCaster
    {
        model = "\PredateQuadNodShadows\GPNVG_Elite_Caster.p3d";
    };
};
