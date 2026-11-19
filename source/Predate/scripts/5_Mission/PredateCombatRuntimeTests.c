// Measures real firearm events and native projectile body-impact callbacks.
// A validation-only target callback prevents damage; aim/projectiles stay native.
class PredateCombatProbeReport
{
    int Schema = 1;
    float Range;
    float Elapsed;
    float VisualConfirmation;
    int InitialAmmo;
    int RemainingAmmo;
    bool Lethal;
    bool DamageEnabled;
    bool TargetDied;
    float TargetHealth;
    float TargetBlood;
    float TargetShock;
    bool TargetUnconscious;
    float FirstIncapacitation = -1;
    bool MovingTarget;
    string TargetKind = "connected human";
    float TargetTravel;
    int UnexpectedItemSelections;
    int Shots;
    int BodyImpacts;
    int MovingBodyImpacts;
    int PhysicalDamageCallbacks;
    int PhysicalPredictionRecords;
    ref array<float> ShotTimes = new array<float>;
    ref array<float> FireErrors = new array<float>;
    ref array<string> ShotPaths = new array<string>;
    ref array<float> ObservedSpeeds = new array<float>;
    ref array<float> LeadLengths = new array<float>;
    int Checks;
    int Failures;
    int RoundBudget;
    float EmptyAmmoHeldSeconds;
    bool EmptyReloadIdle;
}

class PredateCombatRuntimeTests
{
    PredateSession Session;
    PlayerBase Target;
    eAIBase Shooter;
    vector Start;
    vector TargetPosition;
    float Started;
    float FirstVisual = -1;
    float CoverAt;
    int ShotsBeforeCover;
    bool Covered;
    bool CoverSettled;
    bool DrainingProjectiles;
    float DrainBegan;
    ref PredateCombatProbeReport Report = new PredateCombatProbeReport;
    int Checks;
    int Failures;
    float NextDiagnostic;
    bool CheckedNativeControl;
    int LaneSkip;
    int NativeLaneAttempts;
    bool TestExposure;
    bool TestLethal;
    bool TestDamage;
    bool TestMoving;
    int RoundBudget;
    float EmptyAmmoAt = -1;
    vector LastTargetPosition;
    float TargetTravel;
    int UnexpectedItemSelections;
    bool Measured;
    float FirstIncapacitation = -1;
    Object ExposureWall;
    eAIBase MovingActor;

    void VerifyPhysicalOnlyGuard()
    {
        Weapon_Base weapon = Weapon_Base.Cast(Shooter.GetHumanInventory().GetEntityInHands());
        Check(weapon != null, "physical-only guard fixture retains its native firearm");
        if (!weapon) return;
        vector origin = Shooter.GetBonePositionWS(Shooter.GetBoneIndexByName("neck"));
        vector point = Target.GetPosition() + "0 1 0";
        eAIShot hypothesis = new eAIShot(weapon, weapon.GetCurrentMuzzle(), origin, vector.Direction(origin, point).Normalized(), Target, point, -1);
        hypothesis.m_PredatePhysicalMotionOnly = true;
        float health = Target.GetHealth("", "Health");
        int hits = Target.m_PredateProbeHits;
        int ammunition = CountAmmo(Shooter);
        bool redirected = Target.m_eAI_DamageHandler.CheckCandidate(hypothesis, Shooter, "0 1 0", "1 0 0", 0, "Torso");
        Check(!redirected && hypothesis.m_ProcessedTime == 0, "a motion hypothesis cannot redirect damage without an actual body collision");
        Check(Target.GetHealth("", "Health") == health && Target.m_PredateProbeHits == hits && CountAmmo(Shooter) == ammunition, "the unfired hypothesis causes no damage, hit or ammunition change");
    }

    void SetTargetMotion(bool active)
    {
        if (!TestMoving) return;
        if (!MovingActor) return;
        eAIGroup group = MovingActor.GetGroup();
        group.ClearWaypoints();
        if (active)
        {
            float heading = PredateMath.Bearing(Start, TargetPosition) + 90;
            vector sideways = PredateInformationMath.BearingDirection(heading) * 25;
            vector left = BenchGround(TargetPosition - sideways);
            vector right = BenchGround(TargetPosition + sideways);
            vector leftNav, rightNav;
            bool route = PredateRoutePlanner.Snap(left, 10, leftNav) && PredateRoutePlanner.Snap(right, 10, rightNav);
            Check(route, "moving target has a real native navigation corridor");
            if (!route) return;
            group.AddWaypoint(leftNav);
            group.AddWaypoint(rightNav);
            group.SetWaypointBehaviour(eAIWaypointBehavior.LOOP);
            MovingActor.SetMovementSpeedLimits(2, 2);
            MovingActor.DisableSimulation(false);
        }
        else
        {
            group.SetWaypointBehaviour(eAIWaypointBehavior.HALT);
            MovingActor.SetMovementSpeedLimits(0, 0);
        }
    }

    void BeginHardCover()
    {
        vector midpoint = BenchGround(Start + (TargetPosition - Start) * 0.5);
        Object wall = GetGame().CreateObjectEx("Land_Mil_Barracks6", midpoint, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
        if (wall) wall.SetOrientation("45 0 0");
        Check(wall != null, "hard-cover probe creates native view geometry");
        Covered = true;
        float now = GetGame().GetTickTime();
        CoverAt = now;
    }

    void BeginExposure()
    {
        string wallClass;
        array<string> candidates = {"Land_Mil_WallBig_4m", "Land_Mil_WallBig_4m_Damaged", "WoodenCrate", "Barrel_Blue"};
        foreach (string candidate : candidates)
            if (GetGame().ConfigIsExisting("CfgVehicles " + candidate)) { wallClass = candidate; break; }
        Check(wallClass != "", "a native low-cover fixture asset is available");
        if (wallClass == "") { BeginHardCover(); return; }
        vector middle = BenchGround(Start + (TargetPosition - Start) * 0.5);
        ExposureWall = GetGame().CreateObjectEx(wallClass, middle, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
        vector box[2];
        bool bounds = ExposureWall && ExposureWall.GetCollisionBox(box);
        Check(bounds, "native low-cover geometry exposes measurable bounds");
        if (!bounds) { BeginHardCover(); return; }
        float yaw = PredateMath.Bearing(Start, TargetPosition);
        if (box[1][2] - box[0][2] > box[1][0] - box[0][0]) yaw += 90;
        ExposureWall.SetOrientation(Vector(yaw, 0, 0));
        vector eyes = Shooter.GetBonePositionWS(Shooter.GetBoneIndexByName("neck"));
        vector rawOffset = Shooter.GetTarget().m_PredateObservedAimOffset;
        float neckLine = (eyes[1] + TargetPosition[1] + rawOffset[1]) * 0.5;
        middle[1] = neckLine - 0.085 - box[1][1];
        ExposureWall.SetPosition(middle);
        // Portable fallback assets are dynamic. Hold this authored test obstacle
        // in place so gravity cannot turn an exposure check into an empty lane.
        Physics coverPhysics = ExposureWall.GetPhysics();
        if (coverPhysics) coverPhysics.ChangeSimulationState(SimulationState.COLLISION);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(VerifyExposure, 1000, false);
    }

    void VerifyExposure()
    {
        eAITarget state = Shooter.GetTarget();
        vector rawOffset = state.m_PredateObservedAimOffset;
        vector chestOffset = rawOffset;
        chestOffset[1] = chestOffset[1] - 0.25;
        Weapon_Base weapon = Weapon_Base.Cast(Shooter.GetHumanInventory().GetEntityInHands());
        bool chestClear = PredateShotLane.Clear(Shooter, weapon, TargetPosition + chestOffset, Target);
        bool neckClear = PredateShotLane.Clear(Shooter, weapon, TargetPosition + rawOffset, Target);
        Check(!chestClear && neckClear, "real partial cover blocks the chest but leaves the neck firing lane clear");
        Shooter.m_PredateNextPerceptionAt = 0;
        Shooter.m_eAI_TargetChanged = true;
        Check(Shooter.EnforceLOS(1), "native sensor still sees the exposed head above partial cover");
        vector selectedOffset = Target.GetTargetInformation().GetAimOffset(Shooter);
        vector observedOffset = state.m_PredateObservedAimOffset;
        Check(vector.Distance(selectedOffset, observedOffset) < 0.02, "combat selects the observed head when the chest is covered");
        BeginHardCover();
    }

    int CountAmmo(PlayerBase person)
    {
        array<EntityAI> inventory = new array<EntityAI>;
        person.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        Weapon_Base rifle = Weapon_Base.Cast(person.GetHumanInventory().GetEntityInHands());
        array<Weapon_Base> carriedWeapons = new array<Weapon_Base>;
        int rounds;
        foreach (EntityAI item : inventory)
        {
            Magazine magazine = Magazine.Cast(item);
            if (magazine) rounds += magazine.GetAmmoCount();
            Weapon_Base carried = Weapon_Base.Cast(item);
            if (carried && carriedWeapons.Find(carried) < 0) carriedWeapons.Insert(carried);
        }
        if (rifle && carriedWeapons.Find(rifle) < 0) carriedWeapons.Insert(rifle);
        foreach (Weapon_Base ownedWeapon : carriedWeapons)
        {
            for (int muzzle = 0; muzzle < ownedWeapon.GetMuzzleCount(); muzzle++)
            {
                Magazine attached = ownedWeapon.GetMagazine(muzzle);
                if (attached && inventory.Find(attached) < 0) rounds += attached.GetAmmoCount();
                if (!ownedWeapon.IsChamberEmpty(muzzle) && !ownedWeapon.IsChamberFiredOut(muzzle)) rounds++;
                rounds += ownedWeapon.GetInternalMagazineCartridgeCount(muzzle);
            }
        }
        return rounds;
    }

    void Check(bool condition, string label)
    {
        Checks++;
        if (condition) return;
        Failures++;
        Print("[PredateCombatTest] FAIL: " + label);
    }

    vector BenchGround(vector position)
    {
        position[1] = GetGame().SurfaceRoadY(position[0], position[2]);
        return position;
    }

    bool ClearLane(vector from, vector to, Object ignore)
    {
        set<Object> objects = new set<Object>;
        vector contact, normal;
        int component;
        // Vegetation can expose animated collision rather than ONLYSTATIC.
        // Match the native sensor's object mask so bushes cannot pass the
        // fixture's lane test and then correctly block the actual operator.
        bool hit = DayZPhysics.RaycastRV(from, to, contact, normal, component, objects, null, ignore, false, false, ObjIntersectView, 0.1, CollisionFlags.NEARESTCONTACT);
        if (!hit) return true;
        if (objects.Count() == 0) return false; // Terrain before the endpoint.
        foreach (Object object : objects)
        {
            if (object == Target) continue;
            EntityAI entity = EntityAI.Cast(object);
            if (!entity || entity.GetHierarchyRoot() != Target) return false;
        }
        return true; // The target/its own equipment is the expected endpoint.
    }

    void Run(PredateSession session, PlayerBase player)
    {
        Session = session;
        Target = player;
        Shooter = session.m_HuntBot;
        Shooter.eAI_SetLootingBehavior(0);
        vector preload = BenchGround(session.m_HuntConfig.BotStart);
        if (TestMoving)
        {
            // Keep the connected human outside the encounter. The benchmark
            // target uses real native AI locomotion, with no live relocation.
            Object created = GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", preload, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
            eAIBase mover = eAIBase.Cast(created);
            Check(mover != null, "independent native moving target exists");
            if (!mover) { Finish(); return; }
            mover.SetGroup(eAIGroup.CreateGroup(new eAIFactionObservers));
            player.SetAllowDamage(false);
            mover.eAI_SetThreatDistanceLimit(0);
            mover.eAI_SetNoiseInvestigationDistanceLimit(0);
            mover.eAI_SetLootingBehavior(0);
            mover.SetMovementSpeedLimits(0, 0);
            session.PredateEquip(mover);
            Target = mover;
            MovingActor = mover;
            Report.TargetKind = "independent native AI";
        }
        Shooter.SetPosition(preload);
        Target.SetPosition(BenchGround(preload + "17 0 29"));
        Shooter.SetMovementSpeedLimits(0, 0);
        Shooter.m_PredateProbeShooter = true;
        Shooter.m_PredateProbeCeaseFire = true;
        Target.m_PredateProbeTarget = true;
        Target.m_PredateProbeProtect = !(TestLethal || TestDamage);
        Report.Lethal = TestLethal;
        Report.DamageEnabled = TestLethal || TestDamage;
        // Let native static collision geometry stream around the new area
        // before judging a lane. A pre-teleport ray can miss unloaded proxies.
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BeginTrial, 5000, false);
    }

    void BeginTrial()
    {
        PredateSession session = Session;
        // These config coordinates are authored by the disposable test harness.
        vector initial = BenchGround(session.m_HuntConfig.BotStart);
        Start = initial;
        int year, month, day, hour, minute;
        GetGame().GetWorld().GetDate(year, month, day, hour, minute);
        session.m_WorldState.Year = year;
        session.m_WorldState.Month = month;
        session.m_WorldState.Day = day;
        session.m_WorldState.Hour = 12;
        session.m_WorldState.Minute = 0;
        GetGame().GetWorld().SetDate(year, month, day, 12, 0);
        float range = session.m_HuntConfig.PatrolSize;
        Report.Range = range;
        bool lane;
        int passedLanes;
        // A fixed airfield point may sit beside a raised runway proxy. Search
        // the streamed neighbourhood instead of assuming the place name means
        // every sight line is clear. All candidates are authored fixture data.
        for (int east = -2; east <= 2 && !lane; east++)
        {
            for (int north = -2; north <= 2 && !lane; north++)
            {
                Start = BenchGround(initial + Vector(east * 150, 0, north * 150));
                if (!PredateRoutePlanner.Land(Start)) continue;
                for (int heading = 0; heading < 360; heading += 15)
                {
                    vector direction = PredateInformationMath.BearingDirection(heading);
                    vector candidate = BenchGround(Start + direction * range);
                    TargetPosition = candidate;
                    if (!PredateRoutePlanner.Land(TargetPosition)) continue;
                    if (Math.AbsFloat(TargetPosition[1] - Start[1]) > range * 0.05) continue;
                    bool clear = true;
                    for (int a = 0; a < 3 && clear; a++)
                    {
                        for (int b = 0; b < 3 && clear; b++)
                        {
                            vector from = Start + Vector(0, 1.0 + a * 0.4, 0);
                            vector to = TargetPosition + Vector(0, 1.0 + b * 0.4, 0);
                            if (!ClearLane(from, to, Shooter)) clear = false;
                        }
                    }
                    if (!clear) continue;
                    passedLanes++;
                    if (passedLanes <= LaneSkip) continue;
                    lane = true;
                    break;
                }
            }
        }
        Check(lane, "physical view geometry provides a clear test lane");
        if (!lane) { Finish(); return; }
        Shooter.SetAllowDamage(true);
        Shooter.SetHealth("", "Health", 100);
        Shooter.SetHealth("", "Blood", 5000);
        Target.SetAllowDamage(true);
        Target.SetHealth("", "Health", 100);
        Target.SetHealth("", "Blood", 5000);
        Target.SetHealth("", "Shock", 100);
        Target.m_PredateProbeTarget = true;
        Shooter.m_PredateProbeShooter = true;
        Shooter.m_PredateProbeCeaseFire = true;
        Shooter.m_PredateProbeShotTimes.Clear();
        Shooter.m_PredateProbeFireErrors.Clear();
        Shooter.m_PredateProbeShotPaths.Clear();
        Shooter.m_PredateProbeObservedSpeeds.Clear();
        Shooter.m_PredateProbeLeadLengths.Clear();
        Shooter.m_PredateProbePhysicalPredictions = 0;
        Target.m_PredateProbeHits = 0;
        Target.m_PredateProbeMovingHits = 0;
        Target.m_PredateProbePhysicalCallbacks = 0;
        Target.SetPosition(TargetPosition);
        HumanCommandMove targetMove = Target.GetCommand_Move();
        if (targetMove) targetMove.ForceStance(DayZPlayerConstants.STANCEIDX_ERECT);
        Shooter.SetPosition(Start);
        vector look = TargetPosition - Start;
        look[1] = 0;
        look.Normalize();
        Shooter.SetDirection(look);
        if (TestMoving)
        {
            array<Man> observers = new array<Man>;
            GetGame().GetPlayers(observers);
            foreach (Man observer : observers)
            {
                if (!observer.GetIdentity()) continue;
                PlayerBase friendlyObserver = PlayerBase.Cast(observer);
                friendlyObserver.SetGroup(Shooter.GetGroup());
                // Retain a nearby real client to keep native actors/geometry
                // streamed, behind the shooter's field of view.
                observer.SetPosition(BenchGround(Start - look * 100));
            }
            Shooter.GetGroup().SetLeader(Shooter);
            Shooter.DisableSimulation(false);
            MovingActor.DisableSimulation(false);
        }
        Shooter.LookAtPosition(TargetPosition + "0 1.3 0");
        Shooter.AimAtPosition(TargetPosition + "0 1.3 0");
        float headingToTarget = PredateMath.Bearing(Start, TargetPosition);
        Shooter.OverrideTurnTarget(true, headingToTarget);
        Shooter.SetMovementSpeedLimits(0, 0);
        Shooter.GetGroup().ClearWaypoints();
        Shooter.GetGroup().SetWaypointBehaviour(eAIWaypointBehavior.HALT);
        Shooter.m_PredateAppliedGoal = -1;
        Shooter.m_PredateStrategicControl = false;
        Shooter.m_PredateRouteActive = false;
        Shooter.m_PredateLastVisualAt = -1;
        Shooter.m_PredateVisualTarget = null;
        Shooter.m_PredateSeenSince = -1;
        Shooter.m_PredateNextPerceptionAt = 0;
        Shooter.m_PredateAimTarget = null;
        Shooter.m_PredateAimSince = 0;
        Shooter.m_PredateNextShotTime = 0;
        Session.m_WorldState.Contacts.Clear();
        Session.m_WorldState.Observations.Entries.Clear();
        bool newlyCreated;
        eAITargetInformation information = Target.GetTargetInformation();
        eAITarget contact = information.AddAI(Shooter, 120000, true, newlyCreated);
        contact.SetInitial(0.8, TargetPosition);
        contact.m_LOS = false;
        array<ref eAITarget> candidates = Shooter.GetTargets();
        // This is an authored aim benchmark, not a target-choice test. Drop
        // candidates carried over from setup teleports/equipment construction.
        candidates.Clear();
        candidates.Insert(contact);
        Shooter.m_eAI_TargetChanged = true;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(VerifyPreparedLane, 1500, false);
    }

    void VerifyPreparedLane()
    {
        NativeLaneAttempts++;
        Shooter.m_PredateNextPerceptionAt = 0;
        Shooter.m_eAI_TargetChanged = true;
        eAITarget preparedTarget = Shooter.GetTarget();
        if (!preparedTarget || preparedTarget.GetEntity() != Target)
        {
            string wrong = "none";
            if (preparedTarget && preparedTarget.GetEntity()) wrong = preparedTarget.GetEntity().GetType();
            Check(false, "prepared benchmark target retained; selected=" + wrong);
            Finish();
            return;
        }
        bool visible = Shooter.EnforceLOS(1);
        if (!visible)
        {
            Shooter.m_PredateKnowledgeEnabled = false;
            Shooter.m_eAI_TargetChanged = true;
            bool nativeVisible = Shooter.EnforceLOS(1);
            Shooter.m_PredateKnowledgeEnabled = true;
            if (nativeVisible)
            {
                Check(false, "information adapter retains native visibility on a prepared lane");
                Finish();
                return;
            }
            Print("[PredateCombatTest] LANE REJECTED: native pose/geometry control; attempt=" + NativeLaneAttempts + "; origin=" + Start + "; target=" + TargetPosition + "; actual=" + Target.GetPosition() + "; offset=" + Shooter.GetTarget().GetAimOffset() + "; " + Shooter.PredateCombatDiagnostic());
            if (NativeLaneAttempts < 12)
            {
                LaneSkip++;
                BeginTrial();
                return;
            }
            Check(false, "a prepared native firing lane is available");
            Finish();
            return;
        }
        // Measure shooting only after the actual selected actors and their
        // streamed pose have passed the native sensor. Never bypass its result.
        Shooter.m_PredateSeenSince = -1;
        Shooter.m_PredateLastVisualAt = -1;
        Shooter.m_PredateVisualTarget = null;
        if (RoundBudget > 0 && !PrepareRoundBudget()) { Finish(); return; }
        Shooter.m_PredateProbeCeaseFire = false;
        int initialAmmo = CountAmmo(Shooter);
        Report.InitialAmmo = initialAmmo;
        float started = GetGame().GetTickTime();
        Started = started;
        LastTargetPosition = Target.GetPosition();
        Report.MovingTarget = TestMoving;
        SetTargetMotion(true);
        Print("[PredateCombatTest] START: range=" + Report.Range + "; origin=" + Start + "; target=" + TargetPosition);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(UpdateProbe, 250, false);
    }

    void UpdateProbe()
    {
        float now = GetGame().GetTickTime();
        float elapsed = now - Started;
        vector targetNow = Target.GetPosition();
        TargetTravel += vector.Distance(targetNow, LastTargetPosition);
        LastTargetPosition = targetNow;
        if (Target.IsUnconscious() && FirstIncapacitation < 0) FirstIncapacitation = elapsed;
        if (Shooter.GetIsSimulationDisabled()) Shooter.DisableSimulation(false);
        if (MovingActor && MovingActor.GetIsSimulationDisabled()) MovingActor.DisableSimulation(false);
        Shooter.SetMovementSpeedLimits(0, 0);
        if (FirstVisual < 0)
        {
            if (!Shooter.eAI_IsChangingStance() && Shooter.eAI_GetStance() != DayZPlayerConstants.STANCEIDX_ERECT)
                Shooter.OverrideStance(DayZPlayerConstants.STANCEIDX_ERECT, true);
            vector visiblePoint = TargetPosition + "0 1.3 0";
            Shooter.LookAtPosition(visiblePoint);
            Shooter.AimAtPosition(visiblePoint);
        }
        if (Shooter.m_PredateLastVisualAt >= 0 && FirstVisual < 0) FirstVisual = elapsed;
        if (elapsed >= 10 && !CheckedNativeControl && !TestMoving && Shooter.GetTarget())
        {
            CheckedNativeControl = true;
            eAITarget contact = Shooter.GetTarget();
            Shooter.m_PredateKnowledgeEnabled = false;
            Shooter.m_eAI_TargetChanged = true;
            Shooter.m_eAI_LOSCheckDT = 1;
            bool nativeVisible = Shooter.EnforceLOS(1);
            vector actualOffset = contact.GetAimOffset();
            vector eyes = Shooter.GetBonePositionWS(Shooter.GetBoneIndexByName("Head"));
            vector neck = Target.GetBonePositionWS(Target.GetBoneIndexByName("neck"));
            vector along = vector.Direction(eyes, neck).Normalized();
            vector endpoint = neck + along;
            set<Object> hits = new set<Object>;
            vector hitPosition, hitNormal;
            int hitComponent;
            bool physicalHit = DayZPhysics.RaycastRV(eyes, endpoint, hitPosition, hitNormal, hitComponent, hits, null, Shooter, false, false, ObjIntersectView, 0.05);
            string hitTypes;
            foreach (Object object : hits) if (object) hitTypes += object.GetType() + ",";
            Shooter.m_PredateKnowledgeEnabled = true;
            contact.m_LOS = false;
            Print("[PredateCombatTest] SENSOR CONTROL: native=" + nativeVisible + "; offset=" + actualOffset + "; looking=" + Shooter.GetLookDirection() + "; targetPosition=" + Target.GetPosition());
            Print("[PredateCombatTest] PHYSICS CONTROL: hit=" + physicalHit + "; objects=" + hitTypes + "; position=" + hitPosition + "; eyes=" + eyes + "; neck=" + neck);
        }
        int shots = Shooter.m_PredateProbeShotTimes.Count();
        if (RoundBudget > 0)
        {
            if (CountAmmo(Shooter) == 0 && shots > 0)
            {
                if (EmptyAmmoAt < 0) EmptyAmmoAt = now;
            }
            else EmptyAmmoAt = -1;
        }
        eAITarget currentSelection = Shooter.GetTarget();
        if (currentSelection && currentSelection.IsItem() && !currentSelection.m_Info.IsHazard()) UnexpectedItemSelections++;
        if (elapsed >= NextDiagnostic)
        {
            NextDiagnostic = elapsed + 5;
            eAITarget selectedTarget = Shooter.GetTarget();
            string selectedType = "none";
            bool expectedTarget;
            if (selectedTarget && selectedTarget.GetEntity())
            {
                selectedType = selectedTarget.GetEntity().ToString();
                expectedTarget = selectedTarget.GetEntity() == Target;
                ItemBase selectedItem = ItemBase.Cast(selectedTarget.GetEntity());
                if (selectedItem) selectedType += "; root=" + selectedItem.GetHierarchyRootPlayer() + "; previousOwner=" + selectedItem.m_Expansion_PreviousOwner;
            }
            Print("[PredateCombatTest] STATUS: elapsed=" + elapsed + "; shots=" + shots + "; selected=" + selectedType + "; expected=" + expectedTarget + "; targetTravel=" + TargetTravel + "; " + Shooter.PredateCombatDiagnostic());
        }
        bool exhausted = RoundBudget > 0 && EmptyAmmoAt >= 0 && now - EmptyAmmoAt >= 5;
        bool enoughShots = RoundBudget == 0 && !TestLethal && elapsed >= 20 && shots >= 10;
        bool timeLimit = elapsed >= 60;
        if (TestLethal) timeLimit = elapsed >= 180;
        if (!Covered && !DrainingProjectiles && (timeLimit || enoughShots || exhausted || !Target.IsAlive()))
        {
            DrainingProjectiles = true;
            DrainBegan = now;
            Shooter.m_PredateProbeCeaseFire = true;
            SetTargetMotion(false);
        }
        if (!Measured && DrainingProjectiles && now - DrainBegan >= 1)
        {
            Measured = true;
            Check(FirstVisual >= 0, "native sensor acquires a stationary visible target");
            Check(shots > 0, "fire gates allow an actual engagement");
            Check(Target.m_PredateProbeHits <= shots, "deduplicated impact count cannot exceed fired rounds");
            if (!TestLethal && !TestDamage) Check(Target.GetHealth("", "Health") == 100, "fixture protects the body while measuring projectile impacts");
            Print("[PredateCombatTest] MEASURE: elapsed=" + elapsed + "; visualConfirmation=" + FirstVisual + "; shots=" + shots + "; bodyImpacts=" + Target.m_PredateProbeHits + "; deliberateShots=" + Shooter.m_PredateFiredShots);
            Report.Elapsed = elapsed;
            Report.TargetTravel = TargetTravel;
            Report.UnexpectedItemSelections = UnexpectedItemSelections;
            if (TestMoving) Check(UnexpectedItemSelections == 0, "moving-target engagement is not displaced by a non-hazard item target");
            if (TestMoving) Check(TargetTravel >= 10, "actual native locomotion moves the target at least ten metres");
            Report.VisualConfirmation = FirstVisual;
            int remainingAmmo = CountAmmo(Shooter);
            Report.RemainingAmmo = remainingAmmo;
            Check(remainingAmmo <= Report.InitialAmmo - shots, "fired rounds consume finite owned ammunition without refills");
            Report.Shots = shots;
            Report.BodyImpacts = Target.m_PredateProbeHits;
            Report.MovingBodyImpacts = Target.m_PredateProbeMovingHits;
            Report.PhysicalDamageCallbacks = Target.m_PredateProbePhysicalCallbacks;
            Report.PhysicalPredictionRecords = Shooter.m_PredateProbePhysicalPredictions;
            bool died = !Target.IsAlive();
            float health = Target.GetHealth("", "Health");
            float blood = Target.GetHealth("", "Blood");
            Report.TargetDied = died;
            Report.TargetHealth = health;
            Report.TargetBlood = blood;
            float shock = Target.GetHealth("", "Shock");
            bool unconscious = Target.IsUnconscious();
            Report.TargetShock = shock;
            Report.TargetUnconscious = unconscious;
            Report.FirstIncapacitation = FirstIncapacitation;
            foreach (float firedAt : Shooter.m_PredateProbeShotTimes) Report.ShotTimes.Insert(firedAt - Started);
            foreach (float fireError : Shooter.m_PredateProbeFireErrors) Report.FireErrors.Insert(fireError);
            foreach (string shotPath : Shooter.m_PredateProbeShotPaths) Report.ShotPaths.Insert(shotPath);
            foreach (float observedSpeed : Shooter.m_PredateProbeObservedSpeeds) Report.ObservedSpeeds.Insert(observedSpeed);
            int ledShots;
            foreach (float leadLength : Shooter.m_PredateProbeLeadLengths)
            {
                Report.LeadLengths.Insert(leadLength);
                if (leadLength > 0.1) ledShots++;
            }
            if (TestMoving) Check(ledShots > 0, "moving-target shots use bounded observed-motion lead");
            if (TestMoving) Check(Target.m_PredateProbeMovingHits > 0, "a shot taken against observed motion reaches the moving body through native damage handling");
            if (TestMoving) VerifyPhysicalOnlyGuard();
            if (RoundBudget > 0)
            {
                Report.RoundBudget = RoundBudget;
                if (EmptyAmmoAt >= 0) Report.EmptyAmmoHeldSeconds = now - EmptyAmmoAt;
                bool reloadIdle = !Shooter.GetWeaponManager().IsRunning();
                Report.EmptyReloadIdle = reloadIdle;
                Check(shots == RoundBudget && remainingAmmo == 0, "the actor expends exactly its bounded owned ammunition");
                Check(Report.EmptyAmmoHeldSeconds >= 5, "empty ammunition remains empty without unsupported extra shots");
                Check(reloadIdle, "an empty actor exits native weapon actions without a stuck reload");
                Finish();
                return;
            }
            if (TestLethal || TestDamage)
            {
                if (TestLethal) Check(died, "normal native damage can defeat the normally equipped target");
                else Check(health < 99 || blood < 4990 || died, "unprotected native projectiles cause a real wound in normally equipped target");
                Check(Shooter.IsAlive(), "shooter remains alive during the normal-damage fixture");
                Finish();
                return;
            }
            // After motion stops, cover must be placed against the actual final
            // pose; the original spawn coordinate is no longer the target.
            TargetPosition = Target.GetPosition();
            if (TestExposure) BeginExposure();
            else BeginHardCover();
        }
        if (Covered && now - CoverAt >= 1 && !CoverSettled)
        {
            ShotsBeforeCover = shots;
            CoverSettled = true;
            bool blocked = !ClearLane(Start + "0 1.6 0", TargetPosition + "0 1.3 0", Shooter);
            Check(blocked, "native geometry actually blocks the firing lane");
            Check(!Shooter.eAI_HasLOS(), "automatic native perception recognizes the new hard cover");
            Shooter.m_PredateProbeCeaseFire = false;
        }
        if (Covered && now - CoverAt >= 6)
        {
            Check(shots == ShotsBeforeCover, "unsupported firing stops behind hard cover");
            Check(Shooter.IsAlive() && Target.IsAlive(), "both probe actors remain alive");
            Finish();
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(UpdateProbe, 250, false);
    }

    bool PrepareRoundBudget()
    {
        Weapon_Base weapon = Weapon_Base.Cast(Shooter.GetHumanInventory().GetEntityInHands());
        Check(weapon != null, "bounded-ammunition fixture retains a firearm");
        if (!weapon) return false;
        array<EntityAI> inventory = new array<EntityAI>;
        Shooter.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inventory);
        foreach (EntityAI item : inventory)
        {
            Magazine magazine = Magazine.Cast(item);
            if (magazine) magazine.ServerSetAmmoCount(0);
        }
        int chambered;
        for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
            if (!weapon.IsChamberEmpty(muzzle) && !weapon.IsChamberFiredOut(muzzle)) chambered++;
        Magazine attached = weapon.GetMagazine(weapon.GetCurrentMuzzle());
        Check(attached && chambered <= RoundBudget, "native fixture has room for its ammunition budget");
        if (!attached || chambered > RoundBudget) return false;
        attached.ServerSetAmmoCount(RoundBudget - chambered);
        int actual = CountAmmo(Shooter);
        Check(actual == RoundBudget, "all reserve magazines and chambered rounds are included in the finite budget");
        return actual == RoundBudget;
    }

    void Finish()
    {
        SetTargetMotion(false);
        Report.Checks = Checks;
        Report.Failures = Failures;
        string error;
        JsonFileLoader<PredateCombatProbeReport>.SaveFile("$profile:CombatProbeResult.json", Report, error);
        Print("[PredateCombatTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
    }
}
