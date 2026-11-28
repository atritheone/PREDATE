class PredateControl
{
    int Schema = 1;
    string SessionId;
    int Sequence;
    string Action;
}

class PredateCommandFile
{
    static string ReadChanged(string path, string previous)
    {
        if (!FileExist(path)) return "";
        FileHandle file = OpenFile(path, FileMode.READ);
        if (!file) return "";
        string contents;
        ReadFile(file, contents, 8192);
        CloseFile(file);
        if (contents == previous) return "";
        return contents;
    }
}

class PredateSessionStatus
{
    int Schema = 1;
    string SessionId;
    string WorldName;
    string Phase = "booting";
    string Error;
    bool Ready;
    int Players;
    int Acknowledged;
    bool Paused;
    bool PauseReady;
    float SimulationSeconds;
    bool Selected;
    bool Defeated;
    float PulseRemaining;
    float PulseAge;
    float BotHealth;
    float BotBlood;
    float BotShock;
    int BotMagazine;
    vector BotPosition;
    float PlayerHealth;
    float PlayerBlood;
    bool PlayerDamageAllowed;
    bool PlayerSimulationDisabled;
    bool BotSimulationDisabled;
    int PlayerMagazine;
    string PlayerFirearmClass;
    vector PlayerPosition;
    int MemoryCount;
    int PlayerLife;
    bool PlayerAwaitingRedeploy;
    bool PlayerSpawnRejected;
}

class PredateClientStatus
{
    int Schema = 1;
    string SessionId;
    string Phase = "loading";
    string Error;
    int Acknowledged;
    bool Paused;
    bool PauseDisconnected;
    bool PauseMenu;
    int GameState;
    bool StartupArtworkReady;

    bool Matches(PredateClientStatus other)
    {
        if (!other) return false;
        return Phase == other.Phase && Error == other.Error && Acknowledged == other.Acknowledged && Paused == other.Paused && PauseDisconnected == other.PauseDisconnected && PauseMenu == other.PauseMenu && GameState == other.GameState && StartupArtworkReady == other.StartupArtworkReady;
    }

    void Remember(PredateClientStatus other)
    {
        Phase = other.Phase;
        Error = other.Error;
        Acknowledged = other.Acknowledged;
        Paused = other.Paused;
        PauseDisconnected = other.PauseDisconnected;
        PauseMenu = other.PauseMenu;
        GameState = other.GameState;
        StartupArtworkReady = other.StartupArtworkReady;
    }
}

class PredateClientCommand
{
    int Schema = 1;
    string SessionId;
    int Sequence;
    string Action;
    string MapId;
    int Port;
    int QueryPort;
    string Password;
}

// Private engine perception state. Never included in receiver messages.
class PredateContactMemory
{
    string PlayerId;
    vector LastKnownPosition;
    vector SearchPosition;
    float Threat;
    float ActiveThreat;
    int RemainingMilliseconds;
    int SearchUpdates;
    bool SearchOnLOSLost;
}

class PredateWeatherForecast
{
    float Actual;
    float Target;
    float Remaining;
}

class PredateWeatherSnapshot
{
    ref array<ref PredateWeatherForecast> Forecasts = {};
    float DistanceFog;
    float HeightFog;
    float HeightBias;
    bool HadSandstorm;
}

class PredateWorldState
{
    int Schema = 1;
    float SimulationSeconds;
    ref PredateWeatherSnapshot WeatherSnapshot;
    int PlayerLife = 1;
    string PlayerCharacterModel;
    bool PlayerPrimaryAssigned;
    ref map<string, int> PlayerWeaponModules = new map<string, int>;
    float PlayerLifeStartedAt;
    ref PredateSupplyPlan PlayerSupplies = new PredateSupplyPlan;
    bool PlayerAwaitingRedeploy;
    float PlayerLastDeathAt = -1;
    bool HasPlayerDeathPosition;
    vector PlayerDeathPosition;
    ref array<vector> RecentPlayerInsertions = {};
    int InformationSchema;
    int SignalSchema;
    int HunterSchema;
    ref PredateHunterState Hunter;
    ref PredateHuntReceiver HuntReceiver = new PredateHuntReceiver;
    ref PredateSignalTrack PlayerSignal;
    ref PredateSignalTrack BotSignal;
    ref PredateSignatureActivity PlayerActivity;
    ref PredateSignatureActivity BotActivity;
    float PlayerInterceptionUntil;
    float BotInterceptionUntil;
    ref PredateObservationMemory Observations;
    ref PredateFootprintLedger Footprints = new PredateFootprintLedger;
    ref array<ref PredateObservation> PendingSounds = new array<ref PredateObservation>;
    int Year;
    int Month;
    int Day;
    int Hour;
    int Minute;
    int WaypointIndex;
    vector CurrentWaypoint;
    bool Backtracking;
    bool HasBotVitals;
    float BotHealth;
    float BotBlood;
    float BotShock;
    ref array<ref PredateContactMemory> Contacts = new array<ref PredateContactMemory>;
}
