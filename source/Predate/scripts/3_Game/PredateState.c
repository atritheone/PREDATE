class PredateProtocol
{
    static const int VERSION = 22;
    static const int MAX_OPERATORS = 100;
    static const int MAX_BEARING_HISTORY = 4;
    static const int REQUEST = 918601;
    static const int STATE = 918602;
    static const int MENU = 918603;
    static const int SESSION_REQUEST = 918604;
    static const int SESSION_ACK = 918605;
    static const int WEAPON_MODULE_LOG = 918606;
    static const int FOOTPRINT_LOOK = 918608;
    static const int SIGNAL_LOST = 918609;
    static const int REQUEST_SUICIDE = 6;
    // These IDs associate saved actors, receiver tracks and evidence with each slot.
    static ref array<string> s_OperatorIds = {
        "raven-01", "lynx-01", "rook-01", "viper-01", "kestrel-01", "jackal-01",
        "fox-01", "wolf-01", "osprey-01", "marten-01", "hawk-01", "badger-01",
        "kite-01", "otter-01", "sable-01", "heron-01", "falcon-01", "adder-01",
        "eagle-01", "pike-01", "sparrow-01", "stoat-01", "shrike-01", "boar-01",
        "magpie-01", "owl-01", "ferret-01", "salmon-01", "wren-01", "weasel-01",
        "bear-01", "cougar-01", "coyote-01", "dingo-01", "caracal-01", "panther-01", "leopard-01",
        "tiger-01", "jaguar-01", "lion-01", "cheetah-01", "bobcat-01", "mongoose-01", "civet-01",
        "hyena-01", "bison-01", "elk-01", "antelope-01", "ibex-01", "chamois-01", "moose-01",
        "reindeer-01", "stag-01", "ram-01", "buck-01", "oryx-01", "saiga-01", "yak-01",
        "muskox-01", "zebra-01", "crane-01", "kingfisher-01", "buzzard-01", "condor-01", "harrier-01",
        "goshawk-01", "merlin-01", "harpy-01", "griffon-01", "tern-01", "curlew-01", "plover-01",
        "lapwing-01", "snipe-01", "quail-01", "grouse-01", "lark-01", "swift-01", "swallow-01",
        "finch-01", "crossbill-01", "redstart-01", "nightjar-01", "bittern-01", "stork-01", "pelican-01",
        "gannet-01", "petrel-01", "albatross-01", "cormorant-01", "shark-01", "marlin-01", "bonito-01",
        "tuna-01", "sturgeon-01", "grayling-01", "barbel-01", "perch-01", "zander-01", "swordfish-01"
    };
    protected static ref array<string> s_OperatorCallsigns = {
        "Deadbolt", "Blackout", "Static", "Cinder", "Lockjaw", "Cipher", "Vesper", "Shard", "Cutout", "Lowlight",
        "Breach", "Ashfall", "Switchback", "Null", "Coldwire", "Torque", "Latch", "Stray", "Havoc", "Needle",
        "Overcast", "Hush", "Slate", "Standoff", "Flint", "Nightshift", "Rime", "Suture", "Dusk", "Spall",
        "Gravel", "Baffle", "Brim", "Undertow", "Ballast", "Scrim", "Rafter", "Torsion", "Grit", "Tungsten",
        "Longwave", "Squall", "Fuse", "Drift", "Fault", "Riven", "Relay", "Silt", "Wiretap", "Detour",
        "Serrate", "Morrow", "Rampart", "Axle", "Chisel", "Gutter", "Hitch", "Pylon", "Ravel", "Sentry",
        "Tangent", "Brass", "Frostline", "Hemlock", "Keystone", "Spur", "Scour", "Ironclad", "Knell", "Crosstalk",
        "Scaffold", "Plumb", "Quench", "Cobalt", "Solder", "Loam", "Dross", "Flare", "Kerosene", "Wedge",
        "Vortex", "Trace", "Obsidian", "Quiver", "Sluice", "Warden", "Blindside", "Levee", "Omen", "Shiver",
        "Gallows", "Sundown", "Drydock", "Rust", "Kiln", "Hollow", "Spindle", "Soot", "Redline", "Scarp"
    };

    static bool AcceptRequest(int version, string operatorId, int request)
    {
        if (version != VERSION) return false;
        if (request == 1 || request == 2) return OperatorSlot(operatorId) >= 0;
        if (operatorId != "raven-01") return false;
        return request >= 3 && request <= REQUEST_SUICIDE;
    }

    static int OperatorSlot(string id)
    {
        return s_OperatorIds.Find(id);
    }

    static string OperatorAt(int slot)
    {
        if (slot < 0 || slot >= MAX_OPERATORS) return "";
        return s_OperatorIds[slot];
    }

    static string SignatureFor(string id)
    {
        string name = DisplayName(id);
        if (name == "") return "";
        name.ToUpper();
        return name + "-01";
    }

    // The save schema includes the original ID in uppercase as part of its identity.
    static string PersistentSignatureFor(string id)
    {
        if (OperatorSlot(id) < 0) return "";
        id.ToUpper();
        return id;
    }

    static string DisplayName(string id)
    {
        int slot = OperatorSlot(id);
        if (slot < 0 || slot >= s_OperatorCallsigns.Count()) return "";
        return s_OperatorCallsigns[slot];
    }
}

class PredateReceiverSample
{
    int Bearing;
    float Error;
    float Age;
    vector Origin;
    int DistanceBand;
}

class PredateState
{
    string OperatorId = "raven-01";
    bool Development;
    bool CanDeploy;
    string Signature;
    bool Available;
    bool Locked;
    int TrackingChoice;
    int LifeScore;
    bool HasPulse;
    bool CloseApproach;
    int Bearing;
    float Age;
    float NextPulse;
    bool HasPulseOrigin;
    vector PulseOrigin;
    float BearingError;
    bool Intercepted;
    ref array<int> ConfirmedKills = {};
    bool SuppliesRefreshed;
    bool NeedsRedeploy;
    bool DeploymentSpawnRejected;
    bool HasDeploymentPosition;
    vector DeploymentPosition;
    int AvailableOperators = -1;
    int FootprintClue;
    vector FootprintGround;
    int FootprintHeading;
    bool FootprintsEnabled;
    bool OwnFootprint;
    bool InspectingOwnFootprint;
    vector OwnFootprintGround;
    int OwnFootprintHeading;
    ref array<ref PredateReceiverSample> History = new array<ref PredateReceiverSample>;
    ref array<ref PredateBearingPulse> Pulses = new array<ref PredateBearingPulse>;
    string Message = "Establishing receiver link...";

    bool CanShowHistory() { return Locked && !NeedsRedeploy; }

    PredateBearingPulse TrackedPulse()
    {
        if (!HasPulse || !Locked || !Available || CloseApproach) return null;
        foreach (PredateBearingPulse pulse : Pulses)
            if (pulse.OperatorId == OperatorId) return pulse;
        return null;
    }
}

class PredateConfig
{
    static const float DEFAULT_OPERATOR_RESPAWN_DELAY = 7;
    static const float DEFAULT_THREAT_DISTANCE = 1000;
    static const float DEFAULT_HEARING_DISTANCE = 600;
    static const float DEFAULT_AI_ACCURACY_MIN = 0.60;
    static const float DEFAULT_AI_ACCURACY_MAX = 1.0;
    int SchemaVersion;
    int StartHour = -1;
    int StartMinute;
    bool FreezeTimeOfDay;
    float PulseInterval = 60;
    float BearingErrorDegrees = 3;
    float FinalApproachRadius = 200;
    float BotAccuracyMin = DEFAULT_AI_ACCURACY_MIN;
    float BotAccuracyMax = DEFAULT_AI_ACCURACY_MAX;
    int CombatTuningRevision;
    string PlayerLoadout = "m4a1";
    bool RequireModernGear;
    string PlayerOutfit;
    bool HasCustomInventory;
    ref array<ref PredateInventoryNode> PlayerInventory = new array<ref PredateInventoryNode>;
    float ThreatDistance = DEFAULT_THREAT_DISTANCE;
    int OpponentCount = 1;
    ref array<int> OperatorRoster = {};
    bool RandomInitialSpawns;
    vector PlayerStart = "4500 0 6500";
    bool ExplicitPlayerStart;
    vector BotStart = "4780 0 6710";
    vector LynxStart = "4220 0 6880";
    vector RookStart = "5060 0 6060";
    float PatrolSize = 40;
    bool RunSelfTests = true;
    bool EnableFairInformation;
    bool EnableSharedSignals;
    bool EnableStrategicHunter;
    bool EnableFootprints;
    int FootprintRulesVersion;
    bool EnableOperatorRespawns;
    float OperatorRespawnDelay = DEFAULT_OPERATOR_RESPAWN_DELAY;
    int OperatorRespawnRulesVersion;

    string OperatorAtPosition(int position)
    {
        if (OperatorRoster && position >= 0 && position < OperatorRoster.Count())
            return PredateProtocol.OperatorAt(OperatorRoster[position]);
        return PredateProtocol.OperatorAt(position);
    }

    bool ValidOperatorRoster()
    {
        if (!OperatorRoster || OperatorRoster.Count() == 0) return true;
        if (OperatorRoster.Count() != OpponentCount) return false;
        set<int> seen = new set<int>;
        foreach (int slot : OperatorRoster)
        {
            if (PredateProtocol.OperatorAt(slot) == "" || seen.Find(slot) >= 0) return false;
            seen.Insert(slot);
        }
        return true;
    }
}

class PredateInventoryNode
{
    string ClassName;
    int Parent = -1;
    string Placement;
    string Slot;
    int Row = -1;
    int Column = -1;
    bool Flipped;
    float Amount = -1;
}

// Logical identity is independent of the engine's transient group/entity IDs.
class PredateOperatorRecord
{
    static const int INSERTION_HISTORY_SIZE = 3;
    string Id = "raven-01";
    string Signature = "RAVEN-01";
    string StorageKey = "PredateRaven01";
    string LifeState = "unspawned";
    int Deployment = 1;
    float RespawnAt;
    bool HasDeathPosition;
    vector DeathPosition;
    ref array<vector> RecentInsertions = {};
    int EquipmentRevision;
    string RifleKit;
    int PlayerWoundBits;
    float PlayerHealthBeforeInjury;
    int ScoreInjuryLife;
    ref PredateSupplyPlan Supplies = new PredateSupplyPlan;

    void ClearPlayerWoundCredit()
    {
        PlayerWoundBits = 0;
        PlayerHealthBeforeInjury = 0;
        ScoreInjuryLife = 0;
    }

    bool RespawnDue(float now, float delay)
    {
        if (LifeState != "defeated") return false;
        if (RespawnAt <= 0) RespawnAt = now + delay;
        return now >= RespawnAt;
    }

    void NormalizeRespawnClock(float now, float delay)
    {
        if (LifeState == "defeated" && RespawnAt > now + delay) RespawnAt = now + delay;
    }

    bool ValidRedeployment()
    {
        if (!RecentInsertions) RecentInsertions = {};
        if (RecentInsertions.Count() > INSERTION_HISTORY_SIZE) return false;
        if (HasDeathPosition && !PredateInformationMath.FiniteVector(DeathPosition)) return false;
        foreach (vector insertion : RecentInsertions)
            if (!PredateInformationMath.FiniteVector(insertion)) return false;
        return PredateInformationMath.Finite(RespawnAt) && RespawnAt >= 0;
    }

    void RememberInsertion(vector site)
    {
        HasDeathPosition = false;
        RecentInsertions.Insert(site);
        if (RecentInsertions.Count() > INSERTION_HISTORY_SIZE) RecentInsertions.RemoveOrdered(0);
    }
}

class PredateProgress
{
    int SchemaVersion;
    ref PredateOperatorRecord Operator = new PredateOperatorRecord;
    bool Selected;
    bool Defeated;
    bool BotCreated;
    bool HasPulse;
    int LastBearing;
    float PulseAge;
    float PulseRemaining;
    bool HasPulseOrigin;
    vector PulseOrigin;
    float LastBearingError;
    vector MapCenter;
    float MapRadius;
    int BotPlacementRevision;
    int PlayerPlacementRevision;
}

class PredateMath
{
    static float WrapHeading(float degrees)
    {
        while (degrees < 0) degrees += 360;
        while (degrees >= 360) degrees -= 360;
        return degrees;
    }

    static float RelativeBearing(float bearing, float heading)
    {
        float delta = WrapHeading(bearing) - WrapHeading(heading);
        while (delta < -180) delta += 360;
        while (delta >= 180) delta -= 360;
        return delta;
    }

    static float CameraHeading(vector direction)
    {
        return WrapHeading(Math.Atan2(direction[0], direction[2]) * Math.RAD2DEG);
    }

    static string CompassPoint(float heading)
    {
        int sector = Math.Round(WrapHeading(heading) / 45.0);
        sector = sector % 8;
        switch (sector)
        {
            case 0: return "N";
            case 1: return "NE";
            case 2: return "E";
            case 3: return "SE";
            case 4: return "S";
            case 5: return "SW";
            case 6: return "W";
            case 7: return "NW";
        }
        return "N";
    }

    static int Bearing(vector from, vector to)
    {
        vector delta = to - from;
        float degrees = Math.Atan2(delta[0], delta[2]) * Math.RAD2DEG;
        if (degrees < 0) degrees += 360;
        int rounded = Math.Round(degrees);
        return rounded % 360;
    }

    static string BearingText(int bearing)
    {
        return BearingDigits(bearing) + " deg";
    }

    static string BearingDigits(int bearing)
    {
        string result = bearing.ToString();
        if (bearing < 10) result = "00" + result;
        else if (bearing < 100) result = "0" + result;
        return result;
    }
}
