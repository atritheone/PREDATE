class PredateOperatorService
{
    static vector PredateGround(PredateSession s, vector position)
    {
        position[1] = GetGame().SurfaceY(position[0], position[2]);
        return position;
    }

    static void PredateEquip(PredateSession s, PlayerBase player, string kitId = "")
    {
        if (kitId == "" && s.m_HuntConfig.HasCustomInventory && !eAIBase.Cast(player))
        {
            string error;
            if (!PredateCustomInventory.Equip(player, s.m_HuntConfig.PlayerInventory, error))
            {
                player.RemoveAllItems();
                PredatePersistence.Fail(s, error);
                return;
            }
            s.SoloApplyCombatRules(player);
            return;
        }
        if (kitId == "")
        {
            kitId = s.m_HuntConfig.PlayerLoadout;
            if (eAIBase.Cast(player)) kitId = "m4a1";
        }
        PredateStartingKit kit;
        if (s.m_Loadouts) kit = s.m_Loadouts.Find(kitId);
        if (!kit) { PredatePersistence.Fail(s, "Starting kit unavailable: " + kitId); return; }
        // This entry point equips a fresh deployment body. Continuation loads
        // native inventory; it never calls this to refill an existing character.
        player.RemoveAllItems();
        player.GetInventory().CreateInInventory("TTSKOPants");
        player.GetInventory().CreateInInventory("TTsKOJacket_Camo");
        player.GetInventory().CreateInInventory("MilitaryBoots_Black");
        player.GetInventory().CreateInInventory("TacticalGloves_Black");
        player.GetInventory().CreateInInventory("Mich2001Helmet");
        player.GetInventory().CreateInInventory("PlateCarrierVest");
        player.GetInventory().CreateInInventory("AssaultBag_Green");
        Weapon_Base rifle = Weapon_Base.Cast(player.GetHumanInventory().CreateInHands(kit.FirearmClass));
        if (rifle)
        {
            foreach (string attachment : kit.Attachments)
                if (!rifle.GetInventory().CreateAttachment(attachment)) PredatePersistence.Fail(s, "Starting-kit attachment is incompatible: " + attachment);
            rifle.SpawnAttachedMagazine(kit.MagazineClass);
            Magazine attached = rifle.GetMagazine(rifle.GetCurrentMuzzle());
            if (!attached || attached.GetType() != kit.MagazineClass) PredatePersistence.Fail(s, "Starting-kit magazine is incompatible: " + kit.MagazineClass);
        }
        else PredatePersistence.Fail(s, "Starting-kit firearm could not be created");
        for (int i = 0; i < kit.SpareMagazines; i++)
            if (!player.GetInventory().CreateInInventory(kit.MagazineClass)) PredatePersistence.Fail(s, "Starting-kit magazine could not be carried");
        for (int j = 0; j < 4; j++)
            player.GetInventory().CreateInInventory("BandageDressing");
        player.GetInventory().CreateInInventory("Morphine");
        player.GetInventory().CreateInInventory("Epinephrine");
        player.GetInventory().CreateInInventory("Compass");
        player.GetInventory().CreateInInventory("ChernarusMap");
        if (rifle) player.SetQuickBarEntityShortcut(rifle, 0);
        s.SoloApplyCombatRules(player);
    }

    static string RivalKit(PredateOperatorRecord record)
    {
        if (record.RifleKit != "") return record.RifleKit;
        array<string> rotation = {"m4a1", "m4a1", "ak74", "fal", "sv98", "m14", "akm", "ak74"};
        int slot = Math.Max(0, PredateProtocol.s_OperatorIds.Find(record.Id));
        return rotation[(slot + (record.Deployment - 1) * 3) % rotation.Count()];
    }

    static bool EquipRival(PredateSession session, eAIBase actor, PredateOperatorRecord record)
    {
        PredateEquip(session, actor, RivalKit(record));
        if (session.m_HuntError != "") return false;
        if (UpgradeRivalEquipment(session, actor, record)) return true;
        return PredatePersistence.Fail(session, "Cannot equip rifle suppressor, night vision or medical kit for " + record.Id);
    }

    static bool UpgradeRivalEquipment(PredateSession session, eAIBase actor, PredateOperatorRecord record)
    {
        if (record.EquipmentRevision >= 1) return true;
        array<EntityAI> inventory = {};
        actor.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        Weapon_Base rifle;
        PredateStartingKit kit;
        bool hasSaline;
        foreach (EntityAI entity : inventory)
        {
            if (entity.GetType() == "SalineBagIV") hasSaline = true;
            if (rifle) continue;
            Weapon_Base candidate = Weapon_Base.Cast(entity);
            if (!candidate) continue;
            kit = session.m_Loadouts.FindRifle(candidate.GetType());
            if (kit) rifle = candidate;
        }
        bool suppressed = rifle && (rifle.GetAttachedSuppressor() || rifle.GetInventory().CreateAttachment(kit.SuppressorClass));
        EntityAI helmet = actor.FindAttachmentBySlotName("Headgear");
        EntityAI goggles;
        if (helmet)
        {
            goggles = helmet.FindAttachmentBySlotName("NVG");
            if (!goggles) goggles = helmet.GetInventory().CreateAttachment("NVGoggles");
        }
        if (!goggles)
        {
            EntityAI headstrap = actor.FindAttachmentBySlotName("Eyewear");
            if (!headstrap) headstrap = actor.GetInventory().CreateAttachment("NVGHeadstrap");
            if (headstrap)
            {
                goggles = headstrap.FindAttachmentBySlotName("NVG");
                if (!goggles) goggles = headstrap.GetInventory().CreateAttachment("NVGoggles");
            }
        }
        bool powered = goggles && (goggles.FindAttachmentBySlotName("BatteryD") || goggles.GetInventory().CreateAttachment("Battery9V"));
        if (!hasSaline) hasSaline = actor.GetInventory().CreateInInventory("SalineBagIV") != null;
        if (record.Supplies.NextAt > 0 && !record.Supplies.Find("SalineBagIV"))
            PredateSupplyService.AddConfigured(record.Supplies, "SalineBagIV", 1);
        if (!suppressed || !powered || !hasSaline) return false;
        record.RifleKit = kit.Id;
        record.EquipmentRevision = 1;
        return true;
    }

    static void PredateConfigureBot(PredateSession s)
    {
        if (!s.m_HuntBot) return;
        UpgradeRivalEquipment(s, s.m_HuntBot, s.m_HuntProgress.Operator);
        s.m_HuntBot.m_PredateCombatEnabled = true;
        s.m_HuntBot.m_PredateSignatureRegistered = s.m_HuntConfig.EnableSharedSignals;
        s.m_HuntBot.PredateBindInformation(s.m_HuntConfig.EnableFairInformation, s.m_HuntProgress.Operator.Id, s.m_WorldState.Observations, s.m_WorldState.PendingSounds, s.m_WorldState.SimulationSeconds);
        s.m_HuntBot.eAI_SetAccuracy(s.m_HuntConfig.BotAccuracyMin, s.m_HuntConfig.BotAccuracyMax);
        s.m_HuntBot.eAI_SetThreatDistanceLimit(s.m_HuntConfig.ThreatDistance);
        s.m_HuntBot.eAI_SetNoiseInvestigationDistanceLimit(600);
        s.m_HuntBot.eAI_SetDamageMultiplier(1);
        s.m_HuntBot.eAI_SetDamageReceivedMultiplier(1);
        s.m_HuntBot.eAI_SetUnlimitedReload(0);
        s.m_HuntBot.SetMovementSpeedLimits(2, 3);
        if (!s.m_ActorsActive) s.m_HuntBot.SetMovementSpeedLimits(0, 0);
        s.m_HuntBot.DisableSimulation(s.m_Paused || !s.m_ActorsActive);
        s.m_HuntBot.m_eAI_EnableFlankingOutsideCombat = false;
        s.m_HuntBot.m_eAI_MaxFlankingDistance = 200;
        s.m_HuntGroup.SetName(s.m_HuntProgress.Operator.Signature);
        s.m_HuntGroup.m_BaseName = s.m_HuntProgress.Operator.StorageKey;
        s.m_HuntGroup.m_Persist = true;
        if (eAIGroup.s_PersistentGroups.Find(s.m_HuntGroup) == -1)
            eAIGroup.s_PersistentGroups.Insert(s.m_HuntGroup);
        s.m_HuntGroup.ClearWaypoints();
        vector center = s.m_HuntConfig.BotStart;
        s.m_HuntGroup.AddWaypoint(s.PredateGround(center));
        float patrol = s.m_HuntConfig.PatrolSize;
        s.m_HuntGroup.AddWaypoint(s.PredateGround(center + Vector(patrol, 0, 0)));
        s.m_HuntGroup.AddWaypoint(s.PredateGround(center + Vector(patrol, 0, patrol)));
        s.m_HuntGroup.AddWaypoint(s.PredateGround(center + Vector(0, 0, patrol)));
        s.m_HuntGroup.SetWaypointBehaviour(eAIWaypointBehavior.LOOP);
        s.SoloApplyCombatRules(s.m_HuntBot);
    }

    static bool SpawnRaven(PredateSession session, vector position)
    {
        session.m_HuntGroup = eAIGroup.CreateGroup(new eAIFactionRaiders);
        Object created = GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", position, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
        session.m_HuntBot = eAIBase.Cast(created);
        if (!session.m_HuntBot) return PredatePersistence.Fail(session, "Cannot create operator");
        session.m_HuntBot.SetGroup(session.m_HuntGroup);
        if (!EquipRival(session, session.m_HuntBot, session.m_HuntProgress.Operator)) return false;
        session.m_HuntProgress.Operator.LifeState = "alive";
        session.m_HuntProgress.BotCreated = true;
        return true;
    }

    static void PredateStart(PredateSession s)
    {
        if (!s.m_HuntSchemaValid || s.m_HuntError != "") return;
        s.PredateClearWeather();
        if (s.m_WorldState.Year == 0 && s.m_HuntConfig.StartHour >= 0)
        {
            int year, month, day, hour, minute;
            GetGame().GetWorld().GetDate(year, month, day, hour, minute);
            GetGame().GetWorld().SetDate(year, month, day, s.m_HuntConfig.StartHour, s.m_HuntConfig.StartMinute);
        }
        PredateOperatorRecord record = s.m_HuntProgress.Operator;
        bool restoring = record.LifeState == "alive";
        if (record.LifeState != "defeated")
        {
            string groupFile = eAIGroup.GetStorageDirectory(record.StorageKey) + eAIGroup.BASENAME;
            if (record.LifeState == "alive")
            {
                eAIGroup restored;
                if (!FileExist(groupFile) || !eAIGroup.Load(groupFile, restored))
                {
                    PredatePersistence.Fail(s, "Cannot restore " + record.Id + "; native group save missing or unreadable");
                    return;
                }
                s.m_HuntGroup = restored;
                eAIBase restoredActor;
                if (restored.Count() == 1) restoredActor = eAIBase.Cast(restored.GetMember(0));
                s.m_HuntBot = restoredActor;
                if (!s.m_HuntBot || !s.m_HuntBot.IsAlive())
                {
                    PredatePersistence.Fail(s, "Expected exactly one living saved operator " + record.Id);
                    return;
                }
                Print("[Predate] Restored persistent " + record.Id + "; pulse clock retained at " + s.m_HuntProgress.PulseRemaining);
            }
            else
            {
                if (s.m_HuntConfig.ExplicitPlayerStart || s.m_HuntConfig.EnableOperatorRespawns || s.m_HuntConfig.OpponentCount >= 6)
                {
                    array<vector> occupied = {s.m_HuntConfig.PlayerStart};
                    vector insertion;
                    if (!PredateSpawnPlanner.OperatorSite(s, s.m_HuntConfig.PlayerStart, occupied, PredateDeployment.MINIMUM_DISTANCE, insertion))
                    {
                        PredatePersistence.Fail(s, "Could not find a dry, navigable insertion for Raven");
                        return;
                    }
                    s.m_HuntConfig.BotStart = insertion;
                    string configError;
                    if (!JsonFileLoader<PredateConfig>.SaveFile("$profile:PredateConfig.json", s.m_HuntConfig, configError))
                    {
                        PredatePersistence.Fail(s, "Could not save Raven's insertion: " + configError);
                        return;
                    }
                }
                if (!SpawnRaven(s, s.PredateGround(s.m_HuntConfig.BotStart))) return;
                Print("[Predate] Spawned registered operator " + record.Id);
            }
            s.PredateConfigureBot();
            if (!s.m_Operators.Bind(record.Id, s.m_HuntGroup, s.m_HuntBot))
            {
                PredatePersistence.Fail(s, "Operator registry binding rejected");
                return;
            }
            if (s.m_HuntProgress.BotPlacementRevision < 1)
            {
                s.m_HuntBot.SetPosition(s.PredateGround(s.m_HuntConfig.BotStart));
                s.m_HuntProgress.BotPlacementRevision = 1;
            }
        }
        else Print("[Predate] Restored defeated operator " + record.Id + "; no respawn.");
        if (s.m_WorldState && s.m_WorldState.Year > 0)
        {
            GetGame().GetWorld().SetDate(s.m_WorldState.Year, s.m_WorldState.Month, s.m_WorldState.Day, s.m_WorldState.Hour, s.m_WorldState.Minute);
            if (restoring && s.m_HuntGroup)
            {
                s.m_HuntGroup.m_CurrentWaypointIndex = s.m_WorldState.WaypointIndex;
                s.m_HuntGroup.m_CurrentWaypoint = s.m_WorldState.CurrentWaypoint;
                s.m_HuntGroup.m_BackTracking = s.m_WorldState.Backtracking;
            }
            if (restoring && s.m_HuntBot && s.m_WorldState.HasBotVitals)
            {
                bool validVitals = s.m_WorldState.BotHealth > 0 && s.m_WorldState.BotHealth <= s.m_HuntBot.GetMaxHealth("", "Health");
                validVitals = validVitals && s.m_WorldState.BotBlood >= 0 && s.m_WorldState.BotBlood <= s.m_HuntBot.GetMaxHealth("", "Blood");
                validVitals = validVitals && s.m_WorldState.BotShock >= 0 && s.m_WorldState.BotShock <= s.m_HuntBot.GetMaxHealth("", "Shock");
                if (!validVitals) { PredatePersistence.Fail(s, "Saved operator vitals are outside the actor's limits"); return; }
                s.m_HuntBot.SetHealth("", "Health", s.m_WorldState.BotHealth);
                s.m_HuntBot.SetHealth("", "Blood", s.m_WorldState.BotBlood);
                s.m_HuntBot.SetHealth("", "Shock", s.m_WorldState.BotShock);
            }
        }
        if (s.m_HuntConfig.OpponentCount > 1)
        {
            string rosterError;
            if (!s.AttachPeerRoster(s.m_HuntConfig.LynxStart, s.m_HuntConfig.RookStart, rosterError)) return;
        }
        s.m_HuntReady = true;
        s.PredateSave();
        Print("[Predate] SESSION READY: schema=1; operator=" + record.Id + "; state=" + record.LifeState + "; development=" + s.m_Development);
        if (s.m_Development && s.m_HuntConfig.RunSelfTests)
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(s.PredateSelfTest, 2000, false);
    }

    static void PredateResetHunt(PredateSession s, PlayerBase player)
    {
        if (!s.m_Development || !s.m_HuntReady || s.m_HuntError != "" || s.m_PeerRoster) return;
        if (s.m_HuntProgress.Defeated) PredateNewDeployment(s);
        if (!s.m_HuntBot || !s.m_HuntBot.IsAlive()) return;
        s.m_HuntBot.SetPosition(s.PredateGround(s.m_HuntConfig.BotStart));
        player.SetPosition(s.PredateGround(s.m_HuntConfig.PlayerStart));
        s.m_HuntProgress.Selected = false;
        s.m_HuntProgress.HasPulse = false;
        s.m_HuntProgress.PulseAge = 0;
        s.m_HuntProgress.PulseRemaining = 0;
        s.m_HuntProgress.PlayerPlacementRevision = 1;
        PredateSignalService.NewDeployment(s);
        s.PredateConfigureBot();
        s.PredateSave();
        Print("[Predate] Development test range reset; existing living actors retain injuries and equipment.");
    }

    static void ResetRavenLife(PredateSession s)
    {
        if (s.m_HuntGroup)
        {
            s.m_HuntGroup.m_Persist = false;
            eAIGroup.s_PersistentGroups.RemoveItem(s.m_HuntGroup);
        }
        PredateOperatorRecord record = s.m_HuntProgress.Operator;
        PredateOperatorDeaths.Keys.RemoveItem(record.StorageKey);
        record.Deployment++;
        record.StorageKey = "PredateRaven01_d" + record.Deployment.ToString();
        record.LifeState = "unspawned";
        record.RespawnAt = 0;
        record.EquipmentRevision = 0;
        record.RifleKit = "";
        record.Supplies = new PredateSupplyPlan;
        s.m_HuntProgress.Defeated = false;
        s.m_HuntProgress.BotCreated = false;
        s.m_HuntProgress.Selected = false;
        s.m_HuntProgress.HasPulse = false;
        s.m_HuntProgress.PulseAge = 0;
        s.m_HuntProgress.PulseRemaining = 0;
        s.m_HuntBot = null;
        s.m_HuntGroup = null;
        s.m_MemoryRestored = false;
        if (s.m_WorldState)
        {
            s.m_WorldState.Contacts.Clear();
            s.m_WorldState.HasBotVitals = false;
            s.m_WorldState.Observations.Entries.Clear();
            s.m_WorldState.PendingSounds.Clear();
            PredateSignalService.NewDeployment(s);
        }
    }

    static void RespawnRaven(PredateSession s, PlayerBase player)
    {
        if (!s.m_HuntConfig.EnableOperatorRespawns || !s.m_HuntProgress.Defeated) return;
        float now = s.m_WorldState.SimulationSeconds;
        PredateOperatorRecord record = s.m_HuntProgress.Operator;
        if (!record.RespawnDue(now, s.m_HuntConfig.OperatorRespawnDelay)) return;
        array<vector> occupied = {};
        PredateDeployment.OperatorOccupancy(s, player, occupied);
        vector site;
        if (!PredateSpawnPlanner.ReplacementSite(s, s.m_HuntConfig.BotStart, occupied, site))
        {
            record.RespawnAt = now + 15;
            return;
        }
        ResetRavenLife(s);
        if (!SpawnRaven(s, site)) return;
        PredateConfigureBot(s);
        if (!s.m_Operators.Bind(record.Id, s.m_HuntGroup, s.m_HuntBot))
        {
            PredatePersistence.Fail(s, "Replacement operator registry binding rejected");
            return;
        }
        s.PredateSave();
    }

    // The legacy explicit action retains the operation and its player body.
    static void PredateNewDeployment(PredateSession s)
    {
        if (!s.m_HuntReady || s.m_HuntError != "" || !s.m_HuntProgress.Defeated || s.m_PeerRoster || s.m_HuntConfig.EnableOperatorRespawns) return;
        ResetRavenLife(s);
        s.PredateStart();
        Print("[Predate] Explicit replacement deployment " + s.m_HuntProgress.Operator.Deployment + "; player state preserved.");
    }
}
