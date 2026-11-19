class PredateControl
{
    int Schema = 1;
    string SessionId;
    int Sequence;
    string Action;
}

class PredateSessionStatus
{
    int Schema = 1;
    string SessionId;
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
}

class PredateClientCommand
{
    int Schema = 1;
    string SessionId;
    int Sequence;
    string Action;
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

class PredateWorldState
{
    int Schema = 1;
    float SimulationSeconds;
    int PlayerLife = 1;
    float PlayerLifeStartedAt;
    ref PredateSupplyPlan PlayerSupplies = new PredateSupplyPlan;
    bool PlayerAwaitingRedeploy;
    float PlayerLastDeathAt = -1;
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
