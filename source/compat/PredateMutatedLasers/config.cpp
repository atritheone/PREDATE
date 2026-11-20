class CfgPatches
{
    class Predate_MutatedLasers
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"MutatedLasers", "Predate_Prototype"};
    };
};

class CfgMods
{
    class PredateMutatedLasers
    {
        dir = "PredateMutatedLasers";
        name = "PREDATE MutatedLasers Compatibility";
        type = "mod";
        inputs = "PredateMutatedLasers/inputs.xml";
        dependencies[] = {"Game", "World", "Mission"};
        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = {"PredateMutatedLasers/scripts/3_Game"};
            };
            class worldScriptModule
            {
                value = "";
                files[] = {"PredateMutatedLasers/scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"PredateMutatedLasers/scripts/5_Mission"};
            };
        };
    };
};
