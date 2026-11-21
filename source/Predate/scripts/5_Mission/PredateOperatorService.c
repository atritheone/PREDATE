class PredateOperatorService
{
    static vector PredateGround(PredateSession s, vector position)
    {
        position[1] = GetGame().SurfaceY(position[0], position[2]);
        return position;
    }

    static void PredateEquip(PredateSession s, PlayerBase player, string kitId = "", PredateOutfit outfit = null, bool tan = false)
    {
        bool rival = eAIBase.Cast(player) != null;
        if (kitId == "" && s.m_HuntConfig.HasCustomInventory && !rival)
        {
            string error;
            if (!PredateCustomInventory.Equip(player, s.m_HuntConfig.PlayerInventory, error))
            {
                player.RemoveAllItems();
                PredatePersistence.Fail(s, error);
                return;
            }
            array<EntityAI> issued = {};
            player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, issued);
            foreach (EntityAI entity : issued)
                SetDefaultFireMode(Weapon_Base.Cast(entity));
            s.m_WorldState.PlayerPrimaryAssigned = false;
            AssignStartingWeapons(s, player);
            s.SoloApplyCombatRules(player);
            return;
        }
        if (kitId == "")
        {
            kitId = s.m_HuntConfig.PlayerLoadout;
            if (rival) kitId = "m4a1";
        }
        PredateStartingKit kit;
        if (s.m_Loadouts) kit = s.m_Loadouts.Find(kitId);
        if (!kit) { PredatePersistence.Fail(s, "Starting kit unavailable: " + kitId); return; }
        // This entry point equips a fresh deployment body. Continuation loads
        // native inventory; it never calls this to refill an existing character.
        player.RemoveAllItems();
        if (!outfit) outfit = s.m_Loadouts.FindOutfit(s.m_HuntConfig.PlayerOutfit);
        if (!player.GetInventory().CreateAttachment(outfit.Body) || !player.GetInventory().CreateAttachment(outfit.Legs) || !player.GetInventory().CreateAttachment(outfit.Feet))
            PredatePersistence.Fail(s, "Starting-kit clothing could not be equipped: " + kit.Id);
        string mountClass = s.m_Loadouts.NightVisionMount;
        string gogglesClass = kit.NightVisionClass;
        if (gogglesClass == "") gogglesClass = "NVGoggles";
        string vestClass = "PlateCarrierVest";
        string pouchClass = "PlateCarrierPouches";
        string glovesClass = "TacticalGloves_Black";
        if (rival)
        {
            vestClass = "PlateCarrierVest_Black";
            pouchClass = "PlateCarrierPouches_Black";
            mountClass = "PredateOperatorHeadstrap_Black";
            gogglesClass = "PredateOperatorNVG_Black";
            string maskClass = "BalaclavaMask_Black";
            if (tan)
            {
                vestClass = "PlateCarrierVest_Desert";
                pouchClass = "PlateCarrierPouches_Desert";
                mountClass = "PredateOperatorHeadstrap_Tan";
                gogglesClass = "PredateOperatorNVG_Tan";
                glovesClass = "TacticalGloves_Beige";
                maskClass = "BalaclavaMask_Beige";
            }
            if (GetGame().ConfigIsExisting("CfgVehicles FOG_GPNVG_Black") && GetGame().ConfigIsExisting("CfgVehicles FOG_GPNVG_Tan"))
            {
                gogglesClass = "FOG_GPNVG_Black";
                if (tan) gogglesClass = "FOG_GPNVG_Tan";
            }
            if (!player.GetInventory().CreateAttachment(maskClass))
                PredatePersistence.Fail(s, "Starting-kit balaclava could not be equipped");
        }
        else
        {
            if (outfit.Mask != "" && !player.GetInventory().CreateAttachment(outfit.Mask))
                PredatePersistence.Fail(s, "Starting-kit balaclava could not be equipped");
            if (outfit.Vest != "") vestClass = outfit.Vest;
            if (outfit.Pouches != "") pouchClass = outfit.Pouches;
            if (outfit.Gloves != "") glovesClass = outfit.Gloves;
            if (outfit.Eyewear != "") mountClass = outfit.Eyewear;
            if (outfit.NightVision != "") gogglesClass = outfit.NightVision;
            if (outfit.WorkshopNightVision != "" && PredateLoadoutCatalog.ClassExists(outfit.WorkshopNightVision))
                gogglesClass = outfit.WorkshopNightVision;
        }
        if (!player.GetInventory().CreateAttachment(glovesClass))
            PredatePersistence.Fail(s, "Starting-kit gloves could not be equipped");
        if (outfit.Headgear != "") mountClass = outfit.Headgear;
        EntityAI nightVisionMount = player.GetInventory().CreateAttachment(mountClass);
        EntityAI vest = player.GetInventory().CreateAttachment(vestClass);
        if (!vest || !vest.GetInventory().CreateAttachment(pouchClass))
            PredatePersistence.Fail(s, "Starting-kit vest pouches could not be equipped");
        Weapon_Base rifle = Weapon_Base.Cast(player.GetHumanInventory().CreateInHands(kit.FirearmClass));
        if (rifle)
        {
            foreach (string attachment : kit.Attachments)
            {
                EntityAI item = rifle.GetInventory().CreateAttachment(PredateLoadoutCatalog.IssuedAttachment(attachment));
                if (!item) PredatePersistence.Fail(s, "Starting-kit attachment is incompatible: " + attachment);
                else if (kit.NightVisionClass != "" && item.GetInventory().HasAttachmentSlot(InventorySlots.GetSlotIdFromString("BatteryD")) && !InstallBattery(item))
                    PredatePersistence.Fail(s, "Starting-kit accessory could not be powered: " + attachment);
            }
            if (!rifle.GetAttachedSuppressor() && !rifle.GetInventory().CreateAttachment(kit.SuppressorClass))
                PredatePersistence.Fail(s, "Starting-kit suppressor is incompatible: " + kit.SuppressorClass);
            rifle.SpawnAttachedMagazine(kit.MagazineClass);
            Magazine attached = rifle.GetMagazine(rifle.GetCurrentMuzzle());
            if (!attached || attached.GetType() != kit.MagazineClass) PredatePersistence.Fail(s, "Starting-kit magazine is incompatible: " + kit.MagazineClass);
            SetDefaultFireMode(rifle);
        }
        else PredatePersistence.Fail(s, "Starting-kit firearm could not be created");
        EntityAI goggles;
        if (nightVisionMount) goggles = nightVisionMount.GetInventory().CreateAttachment(gogglesClass);
        if (!InstallBattery(goggles)) PredatePersistence.Fail(s, "Starting-kit goggles could not be mounted and powered: " + gogglesClass);
        for (int j = 0; j < 4; j++)
            player.GetInventory().CreateInInventory("BandageDressing");
        player.GetInventory().CreateInInventory("Morphine");
        player.GetInventory().CreateInInventory("Compass");
        player.GetInventory().CreateInInventory("ChernarusMap");
        if (rival) player.GetInventory().CreateInInventory("SalineBagIV");
        for (int i = 0; i < kit.SpareMagazines; i++)
            if (!player.GetInventory().CreateInInventory(kit.MagazineClass)) break;
        if (!EquipSidearm(player, vest, s.m_Loadouts.Sidearm))
            PredatePersistence.Fail(s, "Starting-kit sidearm could not be equipped");
        if (!rival)
        {
            s.m_WorldState.PlayerPrimaryAssigned = false;
            AssignStartingWeapons(s, player);
        }
        s.SoloApplyCombatRules(player);
    }

    static void SetDefaultFireMode(Weapon_Base weapon)
    {
        if (weapon && !weapon.Expansion_SetFireMode(ExpansionFireMode.SemiAuto))
            weapon.Expansion_SetFireMode(ExpansionFireMode.Single);
    }

    static bool EquipSidearm(PlayerBase player, EntityAI vest, PredateStartingSidearm kit)
    {
        if (!vest) return false;
        EntityAI holster = vest.FindAttachmentBySlotName("VestHolster");
        if (!holster) holster = vest.GetInventory().CreateAttachment(kit.HolsterClass);
        if (!holster) return false;
        string firearmClass = kit.FirearmClass;
        string magazineClass = kit.MagazineClass;
        if (kit.HasWorkshopSidearm())
        {
            firearmClass = kit.WorkshopFirearmClass;
            magazineClass = kit.WorkshopMagazineClass;
        }
        EntityAI carried = holster.FindAttachmentBySlotName("Pistol");
        if (!carried) carried = holster.GetInventory().CreateAttachment(firearmClass);
        Weapon_Base pistol = Weapon_Base.Cast(carried);
        if (!pistol || pistol.GetType() != firearmClass) return false;
        if (!pistol.GetAttachedSuppressor() && !pistol.GetInventory().CreateAttachment(kit.SuppressorClass)) return false;
        EntityAI light = pistol.FindAttachmentBySlotName("pistolFlashlight");
        if (!light) light = pistol.GetInventory().CreateAttachment(kit.LightClass);
        if (!InstallBattery(light)) return false;
        if (!pistol.GetMagazine(pistol.GetCurrentMuzzle())) pistol.SpawnAttachedMagazine(magazineClass);
        if (!pistol.GetMagazine(pistol.GetCurrentMuzzle())) return false;
        SetDefaultFireMode(pistol);
        EntityAI pouch = player.FindAttachmentBySlotName("Hips");
        if (!pouch) pouch = player.GetInventory().CreateAttachment(kit.MagazinePouchClass);
        if (!pouch) return false;
        CargoBase cargo = pouch.GetInventory().GetCargo();
        if (!cargo) return false;
        int spares;
        for (int index = 0; index < cargo.GetItemCount(); index++)
        {
            EntityAI item = cargo.GetItem(index);
            if (item && item.GetType() == magazineClass) spares++;
        }
        for (int i = spares; i < kit.SpareMagazines; i++)
        {
            Magazine magazine = Magazine.Cast(pouch.GetInventory().CreateEntityInCargo(magazineClass));
            if (!magazine) return false;
            magazine.ServerSetAmmoCount(magazine.GetAmmoMax());
        }
        return true;
    }

    static void AssignStartingWeapons(PredateSession session, PlayerBase player)
    {
        if (!player.GetIdentity()) return;
        Weapon_Base primary = Weapon_Base.Cast(player.GetEntityInHands());
        Weapon_Base sidearm;
        if (primary && primary.IsKindOf("Pistol_Base"))
        {
            sidearm = primary;
            primary = null;
        }
        if (!primary) primary = Weapon_Base.Cast(player.FindAttachmentBySlotName("Shoulder"));
        array<EntityAI> inventory = {};
        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI entity : inventory)
        {
            Weapon_Base weapon = Weapon_Base.Cast(entity);
            if (!weapon) continue;
            if (weapon.IsKindOf("Pistol_Base"))
            {
                if (!sidearm) sidearm = weapon;
            }
            else if (!primary) primary = weapon;
            if (primary && sidearm) break;
        }
        if (primary && player.GetQuickBarEntity(0) != primary) player.PredateAssignWeaponSlot(primary, 0);
        if (sidearm && player.GetQuickBarEntity(1) != sidearm) player.PredateAssignWeaponSlot(sidearm, 1);
        session.m_WorldState.PlayerPrimaryAssigned = true;
    }

    static bool InstallBattery(EntityAI device)
    {
        return device && (device.FindAttachmentBySlotName("BatteryD") || device.GetInventory().CreateAttachment("Battery9V"));
    }

    static string RivalKit(PredateSession session, PredateOperatorRecord record)
    {
        if (record.RifleKit != "") return record.RifleKit;
        int slot = Math.Max(0, PredateProtocol.s_OperatorIds.Find(record.Id));
        int deploymentOffset = Math.Max(0, record.Deployment - 1);
        array<ref PredateStartingKit> modern = session.m_Loadouts.ModernOperatorKits;
        if (modern.Count() > 0) return modern[(slot + deploymentOffset) % modern.Count()].Id;
        array<string> rotation = {"m4a1", "m4a1", "ak74", "fal", "sv98", "m14", "akm", "ak74"};
        return rotation[(slot + deploymentOffset * 3) % rotation.Count()];
    }

    static bool EquipRival(PredateSession session, eAIBase actor, PredateOperatorRecord record)
    {
        string kitId = RivalKit(session, record);
        int slot = Math.Max(0, PredateProtocol.s_OperatorIds.Find(record.Id));
        PredateEquip(session, actor, kitId, RivalOutfit(session, record, slot), slot % 2 == 1);
        if (session.m_HuntError != "") return false;
        record.RifleKit = kitId;
        if (UpgradeRivalEquipment(session, actor, record, true)) return true;
        return PredatePersistence.Fail(session, "Cannot equip starting gear for " + record.Id);
    }

    static PredateOutfit RivalOutfit(PredateSession session, PredateOperatorRecord record, int slot)
    {
        string selection = session.m_HuntConfig.OperatorOutfit;
        if (selection != "" && selection != "varied") return session.m_Loadouts.FindOutfit(selection);
        int deploymentOffset = Math.Max(0, record.Deployment - 1);
        return session.m_Loadouts.RotatingOutfit(slot + deploymentOffset);
    }

    static bool UpgradeRivalEquipment(PredateSession session, eAIBase actor, PredateOperatorRecord record, bool sidearmIssued = false)
    {
        PredateSupplyService.RemoveEpinephrine(actor, record.Supplies);
        if (record.EquipmentRevision < 1 && !UpgradeRivalProtection(session, actor, record)) return false;
        if (record.EquipmentRevision >= 2) return true;
        PredateStartingSidearm sidearm = session.m_Loadouts.Sidearm;
        if (!sidearmIssued && !EquipSidearm(actor, actor.FindAttachmentBySlotName("Vest"), sidearm)) return false;
        string magazineClass = sidearm.IssuedMagazineClass();
        if (record.Supplies.NextAt > 0 && !record.Supplies.Find(magazineClass))
            PredateSupplyService.AddConfigured(record.Supplies, magazineClass, sidearm.SpareMagazines + 1);
        record.EquipmentRevision = 2;
        return true;
    }

    static bool UpgradeRivalProtection(PredateSession session, eAIBase actor, PredateOperatorRecord record)
    {
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
            kit = session.m_Loadouts.Find(record.RifleKit);
            if (!kit || kit.FirearmClass != candidate.GetType())
                kit = session.m_Loadouts.FindRifle(candidate.GetType());
            if (kit) rifle = candidate;
        }
        bool suppressed = rifle && (rifle.GetAttachedSuppressor() || rifle.GetInventory().CreateAttachment(kit.SuppressorClass));
        EntityAI helmet = actor.FindAttachmentBySlotName("Headgear");
        EntityAI goggles;
        EntityAI headstrap = actor.FindAttachmentBySlotName("Eyewear");
        if (headstrap) goggles = headstrap.FindAttachmentBySlotName("NVG");
        if (!goggles)
        {
            if (helmet) goggles = helmet.FindAttachmentBySlotName("NVG");
            if (!goggles && helmet) goggles = helmet.GetInventory().CreateAttachment("NVGoggles");
            if (!goggles && !headstrap) headstrap = actor.GetInventory().CreateAttachment(session.m_Loadouts.NightVisionMount);
            if (!goggles && headstrap) goggles = headstrap.GetInventory().CreateAttachment("NVGoggles");
        }
        bool powered = InstallBattery(goggles);
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
        s.m_HuntBot.Expansion_SetCanBeLooted(false);
        s.m_HuntBot.m_PredateSignatureRegistered = s.m_HuntConfig.EnableSharedSignals;
        s.m_HuntBot.PredateBindInformation(s.m_HuntConfig.EnableFairInformation, s.m_HuntProgress.Operator.Id, s.m_WorldState.Observations, s.m_WorldState.PendingSounds, s.m_WorldState.SimulationSeconds);
        s.m_HuntBot.eAI_SetAccuracy(s.m_HuntConfig.BotAccuracyMin, s.m_HuntConfig.BotAccuracyMax);
        s.m_HuntBot.eAI_SetThreatDistanceLimit(s.m_HuntConfig.ThreatDistance);
        s.m_HuntBot.eAI_SetNoiseInvestigationDistanceLimit(PredateConfig.DEFAULT_HEARING_DISTANCE);
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
                if (s.m_HuntConfig.RandomInitialSpawns || s.m_HuntConfig.ExplicitPlayerStart || s.m_HuntConfig.EnableOperatorRespawns || s.m_HuntConfig.OpponentCount >= 6)
                {
                    array<vector> occupied = {s.m_HuntConfig.PlayerStart};
                    vector insertion;
                    bool found;
                    if (s.m_HuntConfig.RandomInitialSpawns)
                    {
                        found = PredateSpawnPlanner.InitialRoster(s, s.m_PredateOpeningSites);
                        if (found) insertion = s.m_PredateOpeningSites[0];
                    }
                    else
                        found = PredateSpawnPlanner.OperatorSite(s, s.m_HuntConfig.PlayerStart, occupied, PredateDeployment.MINIMUM_DISTANCE, insertion);
                    if (!found)
                    {
                        PredatePersistence.Fail(s, "Could not find dry, navigable opening positions for the selected roster");
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
        s.m_PredateOpeningSites.Clear();
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
            PredateSignalService.NewDeployment(s, true);
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
