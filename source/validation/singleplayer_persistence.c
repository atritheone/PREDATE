// Disposable two-process persistence experiment. Never loads a normal world.
void main() {}

class PredateLocalActorSnapshot
{
    string Type;
    vector Position;
    float Health;
    float Blood;
    float Shock;
    float RightLeg;
    string FirearmClass;
    string Optic;
    int MagazineRounds;
    bool Chambered;
}

class PredateLocalSaveProbe
{
    int Schema = 1;
    string Phase;
    bool HumanSaved;
    bool BotSaved;
    int HumanEpoch;
    int BotEpoch;
    bool HumanRestored;
    bool BotRestored;
    bool SelectedHuman;
    bool HumanAlive;
    bool HumanHasMoveCommand;
    int HumanCommandId;
    int HumanInstanceType;
    bool HumanSimulationDisabled;
    float HumanResumedTravel;
    bool Multiplayer;
    bool HasIdentity;
    float BotResumedTravel;
    ref PredateLocalActorSnapshot HumanState;
    ref PredateLocalActorSnapshot BotState;
}

class PredateLocalSaveMission : MissionGameplay
{
    PlayerBase m_Human;
    eAIBase m_Bot;
    ref eAIGroup m_Group;
    ref PredateLocalSaveProbe m_Probe = new PredateLocalSaveProbe;
    vector m_RestoredStart;
    vector m_HumanRestoredStart;
    float m_ResumeSeconds;

    override void OnInit()
    {
        super.OnInit();
        eAIBase.s_eAI_FTO = 0;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BeginProbe, 4000, false);
    }

    vector Ground(vector point)
    {
        point[1] = GetGame().SurfaceY(point[0], point[2]);
        return point;
    }

    void Equip(PlayerBase person, int rounds)
    {
        person.GetInventory().CreateInInventory("TTSKOPants");
        person.GetInventory().CreateInInventory("TTsKOJacket_Camo");
        person.GetInventory().CreateInInventory("MilitaryBoots_Black");
        Weapon_Base rifle = Weapon_Base.Cast(person.GetHumanInventory().CreateInHands("M4A1"));
        if (!rifle) return;
        rifle.GetInventory().CreateAttachment("M4_RISHndgrd");
        rifle.GetInventory().CreateAttachment("M4_CQBBttstck");
        rifle.GetInventory().CreateAttachment("ACOGOptic");
        rifle.SpawnAttachedMagazine("Mag_STANAG_30Rnd");
        Magazine magazine = rifle.GetMagazine(0);
        if (magazine) magazine.ServerSetAmmoCount(rounds);
    }

    PredateLocalActorSnapshot Snapshot(PlayerBase person)
    {
        PredateLocalActorSnapshot result = new PredateLocalActorSnapshot;
        if (!person) return result;
        string type = person.GetType();
        vector position = person.GetPosition();
        float health = person.GetHealth("", "Health");
        float blood = person.GetHealth("", "Blood");
        float shock = person.GetHealth("", "Shock");
        float leg = person.GetHealth("RightLeg", "Health");
        result.Type = type;
        result.Position = position;
        result.Health = health;
        result.Blood = blood;
        result.Shock = shock;
        result.RightLeg = leg;
        Weapon_Base weapon = Weapon_Base.Cast(person.GetHumanInventory().GetEntityInHands());
        if (weapon)
        {
            string weaponType = weapon.GetType();
            result.FirearmClass = weaponType;
            bool chambered = weapon.Expansion_IsChambered();
            result.Chambered = chambered;
            Magazine magazine = weapon.GetMagazine(0);
            if (magazine)
            {
                int rounds = magazine.GetAmmoCount();
                result.MagazineRounds = rounds;
            }
            EntityAI optic = weapon.FindAttachmentBySlotName("weaponOptics");
            if (optic)
            {
                string opticType = optic.GetType();
                result.Optic = opticType;
            }
        }
        return result;
    }

    // Restore uses saved time explicitly: real hours spent outside the game
    // must not age the actor's inventory. The dependency is left unchanged.
    bool RestoreHeld(string path, int epoch, out EntityAI entity)
    {
        FileSerializer file = new FileSerializer;
        if (!file.Open(path, FileMode.READ)) return false;
        int previousNow = ExpansionEntityStorageModule.Now;
        ExpansionEntityStorageModule.Now = epoch;
        string basePath = path.Substring(0, path.Length() - 4);
        bool result = ExpansionEntityStorageModule.Restore(file, null, basePath, entity, null, null, null, 0, string.Empty, 0, 0, false);
        ExpansionEntityStorageModule.Now = previousNow;
        file.Close();
        return result;
    }

    // The dependency creates ground roots with CreateObjectEx. That restores
    // a survivor body but does not initialise a controllable local player.
    // Keep its native inventory/store-load phases; create this one root using
    // the game's player API. Fail closed if the pinned file layout changes.
    bool RestoreHuman(string path, int epoch, out EntityAI entity)
    {
        FileSerializer file = new FileSerializer;
        if (!file.Open(path, FileMode.READ)) return false;
        int version, timestamp, location;
        string survivorClass;
        vector position, orientation;
        bool header = file.Read(version) && file.Read(timestamp);
        header = header && version == 13 && timestamp == epoch;
        header = header && file.Read(survivorClass) && file.Read(location);
        header = header && location == InventoryLocationType.GROUND;
        header = header && file.Read(position) && file.Read(orientation);
        // Config vehicle names are not necessarily Enforce script typenames.
        // This authored probe uses exactly one known local-survivor class.
        if (!header || survivorClass != "SurvivorM_Mirek")
        {
            Print("[PredateLocalSave] Header rejected: version=" + version + "; timestamp=" + timestamp + "; class=" + survivorClass + "; location=" + location);
            file.Close();
            return false;
        }
        PlayerBase person = PlayerBase.Cast(GetGame().CreatePlayer(null, survivorClass, position, 0, "NONE"));
        if (!person) { file.Close(); return false; }
        person.SetOrientation(orientation);
        GetGame().SelectPlayer(null, person);
        person.DisableSimulation(true);
        ExpansionEntityStorageModule.Reset();
        ExpansionEntityStorageModule.ResetState();
        int previousNow = ExpansionEntityStorageModule.Now;
        ExpansionEntityStorageModule.Now = epoch;
        ExpansionEntityStorageModule.s_LastRestoredVersion = version;
        ExpansionEntityStorageModule.s_CurrentEntity = person;
        ExpansionEntityStorageModule.s_CurrentRootEntity = person;
        string basePath = path.Substring(0, path.Length() - 4);
        bool result = ExpansionEntityStorageModule.Restore_Phase2(file, basePath, person, null, person, version, string.Empty, 0, 0, false);
        if (result) result = ExpansionEntityStorageModule.Restore_Phase3a(file, person, version);
        if (result) ExpansionEntityStorageModule.Restore_Phase3b(person, 0);
        ExpansionEntityStorageModule.s_CurrentEntity = null;
        ExpansionEntityStorageModule.s_CurrentRootEntity = null;
        ExpansionEntityStorageModule.Now = previousNow;
        file.Close();
        entity = person;
        return result;
    }

    void RestoreVitals(PlayerBase person, PredateLocalActorSnapshot saved)
    {
        person.SetAllowDamage(true);
        // Native entity storage preserves Health/zones, but not Blood/Shock.
        person.SetHealth("", "Blood", saved.Blood);
        person.SetHealth("", "Shock", saved.Shock);
    }

    void ConfigureGroup()
    {
        if (!m_Bot) return;
        m_Group = eAIGroup.CreateGroup(new eAIFactionRaiders);
        m_Bot.SetGroup(m_Group);
        m_Bot.SetMovementSpeedLimits(2, 3);
        m_Bot.eAI_SetThreatDistanceLimit(50);
        m_Bot.m_PredateRouteActive = true;
        vector goal = Ground(m_Bot.GetPosition() + "100 0 0");
        m_Group.AddWaypoint(goal);
        m_Group.SetWaypointBehaviour(eAIWaypointBehavior.HALT);
    }

    void BeginProbe()
    {
        if (FileExist("$profile:PredateLocalCheckpoint.json"))
        {
            PredateLocalSaveProbe saved;
            string error;
            if (!JsonFileLoader<PredateLocalSaveProbe>.LoadFile("$profile:PredateLocalCheckpoint.json", saved, error))
            {
                m_Probe.Phase = "checkpoint-error";
                Finish();
                return;
            }
            EntityAI human, bot;
            bool humanRestored = RestoreHuman("$profile:ExpansionMod/PredateSingleplayer/human.bin", saved.HumanEpoch, human);
            m_Human = PlayerBase.Cast(human);
            if (!humanRestored || !m_Human)
            {
                m_Probe.Phase = "human-restore-error";
                Finish();
                return;
            }
            GetGame().SelectPlayer(null, m_Human);
            bool botRestored = RestoreHeld("$profile:ExpansionMod/PredateSingleplayer/bot.bin", saved.BotEpoch, bot);
            m_Probe.HumanRestored = humanRestored;
            m_Probe.BotRestored = botRestored;
            m_Bot = eAIBase.Cast(bot);
            if (!m_Human || !m_Bot)
            {
                m_Probe.Phase = "restore-error";
                Finish();
                return;
            }
            m_Human.DisableSimulation(true);
            m_Bot.DisableSimulation(true);
            RestoreVitals(m_Human, saved.HumanState);
            RestoreVitals(m_Bot, saved.BotState);
            m_Probe.HumanState = Snapshot(m_Human);
            m_Probe.BotState = Snapshot(m_Bot);
            GetGame().SelectPlayer(null, m_Human);
            m_Human.SetAllowDamage(false);
            m_Bot.SetAllowDamage(false);
            ConfigureGroup();
            vector restored = m_Bot.GetPosition();
            m_RestoredStart = restored;
            vector humanStart = m_Human.GetPosition();
            m_HumanRestoredStart = humanStart;
            m_Human.DisableSimulation(false);
            m_Human.StartCommand_Move();
            m_Bot.DisableSimulation(false);
            m_Probe.Phase = "restoring";
            return;
        }
        m_Human = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", Ground("4500 0 6500"), 0, "NONE"));
        // Expansion's diagnostic pathfinding notices nearby players while
        // constructing an AI. A local human must already be selected then.
        if (m_Human) GetGame().SelectPlayer(null, m_Human);
        m_Bot = eAIBase.Cast(GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", Ground("4780 0 6710"), ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
        if (!m_Human || !m_Bot)
        {
            m_Probe.Phase = "creation-error";
            Finish();
            return;
        }
        GetGame().SelectPlayer(null, m_Human);
        Equip(m_Human, 13);
        Equip(m_Bot, 17);
        m_Human.SetAllowDamage(false);
        m_Bot.SetAllowDamage(false);
        ConfigureGroup();
        m_Probe.Phase = "creating";
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SaveActors, 7000, false);
    }

    void SaveActors()
    {
        m_Human.DisableSimulation(true);
        m_Bot.DisableSimulation(true);
        m_Human.SetAllowDamage(true);
        m_Bot.SetAllowDamage(true);
        m_Human.SetHealth("", "Health", 81);
        m_Human.SetHealth("", "Blood", 4555);
        m_Human.SetHealth("", "Shock", 97);
        m_Human.SetHealth("RightLeg", "Health", 73);
        m_Bot.SetHealth("", "Health", 73);
        m_Bot.SetHealth("", "Blood", 4321);
        m_Bot.SetHealth("", "Shock", 93);
        m_Bot.SetHealth("RightLeg", "Health", 67);
        MakeDirectory("$profile:ExpansionMod");
        MakeDirectory("$profile:ExpansionMod/PredateSingleplayer");
        bool humanSaved = ExpansionEntityStorageModule.SaveToFile(m_Human, "$profile:ExpansionMod/PredateSingleplayer/human.bin");
        int humanEpoch = ExpansionEntityStorageModule.Now;
        bool botSaved = ExpansionEntityStorageModule.SaveToFile(m_Bot, "$profile:ExpansionMod/PredateSingleplayer/bot.bin");
        int botEpoch = ExpansionEntityStorageModule.Now;
        m_Probe.HumanSaved = humanSaved;
        m_Probe.BotSaved = botSaved;
        m_Probe.HumanEpoch = humanEpoch;
        m_Probe.BotEpoch = botEpoch;
        m_Probe.HumanState = Snapshot(m_Human);
        m_Probe.BotState = Snapshot(m_Bot);
        m_Probe.Phase = "saved";
        string error;
        JsonFileLoader<PredateLocalSaveProbe>.SaveFile("$profile:PredateLocalCheckpoint.json", m_Probe, error);
        Finish();
    }

    void Finish()
    {
        string error;
        JsonFileLoader<PredateLocalSaveProbe>.SaveFile("$profile:PredateLocalPersistence.json", m_Probe, error);
        Print("[PredateLocalSave] COMPLETE: " + m_Probe.Phase);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(GetGame().RequestExit, 500, false, 0);
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        if (m_Probe.Phase != "restoring") return;
        if (m_Bot.GetIsSimulationDisabled()) m_Bot.DisableSimulation(false);
        if (m_Human.GetIsSimulationDisabled()) m_Human.DisableSimulation(false);
        m_ResumeSeconds += timeslice;
        if (m_ResumeSeconds < 4)
        {
            m_Human.GetInputController().OverrideMovementSpeed(HumanInputControllerOverrideType.ENABLED, 2);
            m_Human.GetInputController().OverrideMovementAngle(HumanInputControllerOverrideType.ENABLED, 0);
        }
        else
        {
            m_Human.GetInputController().OverrideMovementSpeed(HumanInputControllerOverrideType.DISABLED, 0);
            m_Human.GetInputController().OverrideMovementAngle(HumanInputControllerOverrideType.DISABLED, 0);
        }
        if (m_ResumeSeconds < 8) return;
        bool selected = GetGame().GetPlayer() == m_Human;
        bool alive = m_Human.IsAlive();
        bool moving = m_Human.GetCommand_Move() != null;
        int commandId = m_Human.GetCurrentCommandID();
        int instanceType = m_Human.GetInstanceType();
        bool disabled = m_Human.GetIsSimulationDisabled();
        bool multiplayer = GetGame().IsMultiplayer();
        bool identity = m_Human.GetIdentity() != null;
        float travel = vector.Distance(m_RestoredStart, m_Bot.GetPosition());
        float humanTravel = vector.Distance(m_HumanRestoredStart, m_Human.GetPosition());
        m_Probe.SelectedHuman = selected;
        m_Probe.HumanAlive = alive;
        m_Probe.HumanHasMoveCommand = moving;
        m_Probe.HumanCommandId = commandId;
        m_Probe.HumanInstanceType = instanceType;
        m_Probe.HumanSimulationDisabled = disabled;
        m_Probe.Multiplayer = multiplayer;
        m_Probe.HasIdentity = identity;
        m_Probe.BotResumedTravel = travel;
        m_Probe.HumanResumedTravel = humanTravel;
        m_Probe.Phase = "restored";
        Finish();
    }
}
Mission CreateCustomMission(string path) { return new PredateLocalSaveMission; }
