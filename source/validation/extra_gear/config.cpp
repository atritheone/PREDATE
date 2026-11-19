class CfgPatches
{
    class PredateValidationGear
    {
        units[] = {"PredateValidationCompass"};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Gear_Navigation"};
    };
};

class CfgVehicles
{
    class Compass;
    class PredateValidationCompass : Compass
    {
        scope = 2;
        displayName = "PREDATE validation compass";
    };
};
