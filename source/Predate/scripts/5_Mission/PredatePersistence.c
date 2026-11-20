// Schema zero is the original prototype. Migration retains its fields and native AI key.
class PredatePersistence
{
    static const int SCHEMA = 1;

    static bool Fail(PredateSession s, string message)
    {
        s.m_HuntError = message;
        Print("[Predate] RECOVERY REQUIRED: " + message + "; saved hunt will not be overwritten.");
        return false;
    }

    static bool Load(PredateSession s)
    {
        string error;
        s.m_HuntConfig = new PredateConfig;
        s.m_HuntProgress = new PredateProgress;
        s.m_WorldState = new PredateWorldState;
        if (FileExist("$profile:PredateWorldState.json"))
        {
            PredateWorldState worldState = new PredateWorldState;
            if (!JsonFileLoader<PredateWorldState>.LoadFile("$profile:PredateWorldState.json", worldState, error))
                return Fail(s, "Unreadable session state: " + error);
            if (!worldState) return Fail(s, "Missing session state");
            if (worldState.Schema != 1) return Fail(s, "Unsupported session state schema");
            if (!worldState.Contacts) return Fail(s, "Missing contact memory list");
            s.m_WorldState = worldState;
        }
        if (FileExist("$profile:PredateConfig.json"))
        {
            PredateConfig config = new PredateConfig;
            if (!JsonFileLoader<PredateConfig>.LoadFile("$profile:PredateConfig.json", config, error))
                return Fail(s, "Unreadable configuration: " + error);
            if (!config) return Fail(s, "Missing configuration object");
            s.m_HuntConfig = config;
        }
        if (FileExist("$profile:PredateProgress.json"))
        {
            PredateProgress progress = new PredateProgress;
            if (!JsonFileLoader<PredateProgress>.LoadFile("$profile:PredateProgress.json", progress, error))
                return Fail(s, "Unreadable progress: " + error);
            if (!progress) return Fail(s, "Missing progress object");
            s.m_HuntProgress = progress;
        }
        if (s.m_HuntConfig.SchemaVersion < 0 || s.m_HuntConfig.SchemaVersion > SCHEMA || s.m_HuntProgress.SchemaVersion < 0 || s.m_HuntProgress.SchemaVersion > SCHEMA)
            return Fail(s, "Unsupported config/save schema");
        if (s.m_HuntConfig.StartHour < -1 || s.m_HuntConfig.StartHour > 23 || s.m_HuntConfig.StartMinute < 0 || s.m_HuntConfig.StartMinute > 59)
            return Fail(s, "Invalid operation start time");
        PredateLoadoutCatalog loadouts = new PredateLoadoutCatalog;
        if (!JsonFileLoader<PredateLoadoutCatalog>.LoadFile("Predate/data/loadouts.json", loadouts, error) || !loadouts)
            return Fail(s, "Starting-kit catalog could not be loaded: " + error);
        if (!loadouts.LoadModernOperatorKits(s.m_HuntConfig.RequireModernGear)) return Fail(s, loadouts.ValidationError);
        if (!loadouts.Valid()) return Fail(s, "Invalid starting-kit catalog: " + loadouts.ValidationError);
        if (!loadouts.FindOutfit(s.m_HuntConfig.PlayerOutfit)) return Fail(s, "Unknown player clothing preset: " + s.m_HuntConfig.PlayerOutfit);
        string operatorOutfit = s.m_HuntConfig.OperatorOutfit;
        if (operatorOutfit != "" && operatorOutfit != "varied" && !loadouts.FindOutfit(operatorOutfit))
            return Fail(s, "Unknown AI clothing preset: " + operatorOutfit);
        if (s.m_HuntConfig.HasCustomInventory)
        {
            if (!PredateCustomInventory.Validate(s.m_HuntConfig.PlayerInventory, error)) return Fail(s, error);
        }
        else if (!loadouts.Find(s.m_HuntConfig.PlayerLoadout)) return Fail(s, "Unknown starting kit: " + s.m_HuntConfig.PlayerLoadout);
        s.m_Loadouts = loadouts;
        if (s.m_HuntProgress.SchemaVersion == 0)
        {
            s.m_HuntProgress.Operator = new PredateOperatorRecord;
            if (s.m_HuntProgress.Defeated) s.m_HuntProgress.Operator.LifeState = "defeated";
            else if (s.m_HuntProgress.BotCreated) s.m_HuntProgress.Operator.LifeState = "alive";
            Print("[Predate] Migrated prototype save to schema 1; retained raven-01 / PredateRaven01.");
        }
        PredateOperatorRecord record = s.m_HuntProgress.Operator;
        if (!record) return Fail(s, "Missing persistent operator record");
        if (!PredateInformationMath.Finite(s.m_WorldState.SimulationSeconds) || s.m_WorldState.SimulationSeconds < 0)
            return Fail(s, "Invalid simulation clock");
        if (s.m_WorldState.PlayerLife < 1 || !PredateInformationMath.Finite(s.m_WorldState.PlayerLastDeathAt) || s.m_WorldState.PlayerLastDeathAt < -1 || s.m_WorldState.PlayerLastDeathAt > s.m_WorldState.SimulationSeconds || (s.m_WorldState.HasPlayerDeathPosition && !PredateInformationMath.FiniteVector(s.m_WorldState.PlayerDeathPosition)))
            return Fail(s, "Invalid player deployment record");
        if (!PredateInformationMath.Finite(s.m_WorldState.PlayerLifeStartedAt) || s.m_WorldState.PlayerLifeStartedAt < 0 || s.m_WorldState.PlayerLifeStartedAt > s.m_WorldState.SimulationSeconds)
            return Fail(s, "Invalid player deployment start time");
        if (!s.m_WorldState.PlayerSupplies) s.m_WorldState.PlayerSupplies = new PredateSupplyPlan;
        if (!record.Supplies) record.Supplies = new PredateSupplyPlan;
        if (!s.m_WorldState.PlayerSupplies.Valid(s.m_WorldState.SimulationSeconds) || !record.Supplies.Valid(s.m_WorldState.SimulationSeconds))
            return Fail(s, "Invalid saved resupply plan");
        if (s.m_HuntConfig.EnableSharedSignals && !s.m_HuntConfig.EnableFairInformation)
            return Fail(s, "Shared signals require the fair information policy");
        if (s.m_HuntConfig.EnableStrategicHunter && !s.m_HuntConfig.EnableSharedSignals)
            return Fail(s, "Strategic hunter requires shared signals");
        int opponentCount = s.m_HuntConfig.OpponentCount;
        if (opponentCount != 1 && opponentCount != 3 && opponentCount != 6 && opponentCount != 15 && opponentCount != 30)
            return Fail(s, "Unsupported opponent count");
        if (opponentCount > 1 && !s.m_HuntConfig.EnableStrategicHunter)
            return Fail(s, "Multi-opponent operations require strategic hunters");
        if (s.m_HuntConfig.ExplicitPlayerStart && s.m_HuntProgress.PlayerPlacementRevision == 0 && !PredateSpawnPlanner.PlayerTerrain(s.m_HuntConfig.PlayerStart))
            return Fail(s, "The selected spawn is outside usable Chernarus ground. Choose a dry, level point for a new operation.");
        if (s.m_HuntConfig.EnableFootprints && !s.m_HuntConfig.EnableStrategicHunter)
            return Fail(s, "Footprint observations require strategic hunters");
        if (!PredateInformationMath.Finite(record.RespawnAt) || record.RespawnAt < 0)
            return Fail(s, "Invalid operator respawn clock");
        s.m_HuntConfig.OperatorRespawnDelay = Math.Clamp(s.m_HuntConfig.OperatorRespawnDelay, 1, 3600);
        if (!s.m_WorldState.Footprints) s.m_WorldState.Footprints = new PredateFootprintLedger;
        if (!s.m_WorldState.Footprints.Validate(s.m_WorldState.SimulationSeconds))
            return Fail(s, "Invalid saved footprint observations");
        if (s.m_WorldState.InformationSchema == 0)
        {
            PredateObservationMemory legacyInformation = s.m_WorldState.Observations;
            if (legacyInformation && (legacyInformation.OwnerId != "" || (legacyInformation.Entries && legacyInformation.Entries.Count() > 0)))
                return Fail(s, "Private information data has no schema");
            PredateObservationMemory migrated = new PredateObservationMemory;
            migrated.OwnerId = record.Id;
            s.m_WorldState.Observations = migrated;
            s.m_WorldState.InformationSchema = 1;
        }
        if (s.m_WorldState.InformationSchema != 1 || !s.m_WorldState.Observations) return Fail(s, "Unsupported private information schema");
        if (s.m_WorldState.Observations.Entries) s.m_WorldState.Observations.AnonymizeSightings();
        if (s.m_WorldState.Observations.OwnerId != record.Id || !s.m_WorldState.Observations.Validate(s.m_WorldState.SimulationSeconds))
            return Fail(s, "Invalid private observation memory");
        if (!s.m_WorldState.PendingSounds) s.m_WorldState.PendingSounds = new array<ref PredateObservation>;
        if (s.m_WorldState.PendingSounds.Count() > 8) return Fail(s, "Too many pending perception events");
        foreach (PredateObservation pendingSound : s.m_WorldState.PendingSounds)
        {
            if (!pendingSound || !pendingSound.Valid(record.Id, s.m_WorldState.SimulationSeconds + 3)) return Fail(s, "Invalid pending perception event");
            if (pendingSound.Source != PredateObservationSource.AUDIBLE && pendingSound.Source != PredateObservationSource.IMPACT) return Fail(s, "Invalid pending perception source");
        }
        if (record.Id != "raven-01" || record.StorageKey == "" || record.Deployment < 1)
            return Fail(s, "Invalid persistent operator identity");
        string expectedKey = "PredateRaven01";
        if (record.Deployment > 1) expectedKey += "_d" + record.Deployment.ToString();
        if (record.StorageKey != expectedKey)
            return Fail(s, "Unexpected operator storage key");
        if (record.LifeState != "alive" && record.LifeState != "defeated" && record.LifeState != "unspawned")
            return Fail(s, "Unknown operator life state");
        if (s.m_HuntProgress.SchemaVersion == SCHEMA)
        {
            if (s.m_HuntProgress.Defeated != (record.LifeState == "defeated"))
                return Fail(s, "Conflicting operator life state");
            if (s.m_HuntProgress.BotCreated != (record.LifeState != "unspawned"))
                return Fail(s, "Conflicting operator creation state");
        }
        if (s.m_HuntConfig.CombatTuningRevision < 1)
        {
            s.m_HuntConfig.BotAccuracyMin = 0.80;
            s.m_HuntConfig.BotAccuracyMax = 0.95;
            s.m_HuntConfig.CombatTuningRevision = 1;
        }
        s.m_HuntConfig.PulseInterval = Math.Clamp(s.m_HuntConfig.PulseInterval, 10, 600);
        s.m_HuntConfig.FinalApproachRadius = Math.Clamp(s.m_HuntConfig.FinalApproachRadius, 1, 2000);
        if (s.m_HuntProgress.MapRadius != 0 && !PredatePulseArea.Valid(s.m_HuntProgress.MapCenter, s.m_HuntProgress.MapRadius))
            return Fail(s, "Invalid saved pulse area");
        s.m_HuntProgress.PulseRemaining = Math.Clamp(s.m_HuntProgress.PulseRemaining, 0, PredateBearingPulse.INTERVAL);
        if (!PredateSignalService.Load(s)) return false;
        if (!PredateHunterService.Load(s)) return false;
        s.m_HuntConfig.SchemaVersion = SCHEMA;
        s.m_HuntProgress.SchemaVersion = SCHEMA;
        if (!JsonFileLoader<PredateConfig>.SaveFile("$profile:PredateConfig.json", s.m_HuntConfig, error))
            return Fail(s, "Cannot save migrated config: " + error);
        return true;
    }

    static void Save(PredateSession s)
    {
        if (!s.m_HuntSchemaValid || !s.m_HuntReady || s.m_HuntError != "") return;
        // A quit can arrive between a lethal hit and the next simulation tick.
        if (!s.m_HuntProgress.Defeated && (PredateOperatorDeaths.Contains(s.m_HuntProgress.Operator.StorageKey) || (s.m_HuntBot && !s.m_HuntBot.IsAlive())))
        {
            s.m_HuntProgress.Defeated = true;
            s.m_HuntProgress.Operator.LifeState = "defeated";
            s.m_HuntProgress.Selected = false;
            s.m_HuntProgress.HasPulse = false;
        }
        if (s.m_HuntGroup && s.m_HuntBot && s.m_HuntBot.IsAlive()) s.m_HuntGroup.Save(true);
        PredateSignalService.Capture(s);
        PredateMemoryPersistence.Capture(s);
        string error;
        if (s.m_PeerRoster && !s.m_PeerRoster.Save(s, error))
        {
            Fail(s, "Cannot save operator roster: " + error);
            return;
        }
        if (!JsonFileLoader<PredateWorldState>.SaveFile("$profile:PredateWorldState.json", s.m_WorldState, error))
        {
            Fail(s, "Cannot save session state: " + error);
            return;
        }
        if (!JsonFileLoader<PredateProgress>.SaveFile("$profile:PredateProgress.json", s.m_HuntProgress, error))
            Fail(s, "Cannot save progress: " + error);
    }
}
