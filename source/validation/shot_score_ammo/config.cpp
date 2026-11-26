class CfgPatches
{
    class PredateShotScoreProbe
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Weapons_Ammunition", "PredateStockPrecision", "Predate_Prototype"};
    };
};

class CfgMods
{
    class PredateShotScoreProbe
    {
        type = "mod";
        dependencies[] = {"World"};
        class defs
        {
            class worldScriptModule
            {
                value = "";
                files[] = {"shot_score_ammo/scripts"};
            };
        };
    };
};

class CfgAmmo
{
    class Bullet_556x45;
    class PredateProbe556_Sky: Bullet_556x45 {};
    class PredateProbe556_Body: Bullet_556x45 {};
    class PredateProbe556_Air: Bullet_556x45 {};
};
