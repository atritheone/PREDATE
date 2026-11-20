// Static native perception/privacy probes, not a movement or combat benchmark.
class PredateTacticalKnowledgeTests : PredateKnowledgeTests
{
    int VisibleAttempts;
    PredateSession Session;
    eAIBase Observer;
    PlayerBase Target;
    eAITarget State;

    void CheckSightRange()
    {
        int year, month, day, hour, minute;
        GetGame().GetWorld().GetDate(year, month, day, hour, minute);
        float previousLimit = Observer.m_eAI_ThreatDistanceLimit;
        Observer.eAI_SetThreatDistanceLimit(PredateConfig.DEFAULT_THREAT_DISTANCE);
        GetGame().GetWorld().SetDate(year, month, day, 12, 0);
        Observer.Expansion_UpdateVisibility(true);
        float daylight = Observer.eAI_GetVisibilityDistanceLimit(Target);
        GetGame().GetWorld().SetDate(year, month, day, 0, 0);
        Observer.Expansion_UpdateVisibility(true);
        Check(Math.AbsFloat(daylight - 1000) < 0.5, "clear daylight reaches the 1 km character sight cap, measured " + daylight);
        Print("[PredateTacticalTest] DAYLIGHT SIGHT RANGE: " + daylight);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckNightSightRange, 1500, false, year, month, day, hour, minute, previousLimit);
    }

    void CheckNightSightRange(int year, int month, int day, int hour, int minute, float previousLimit)
    {
        Observer.Expansion_UpdateVisibility();
        float night = Observer.eAI_GetVisibilityDistanceLimit(Target);
        NVGoggles goggles = Observer.PredateMountedGoggles();
        Check(goggles && goggles.eAI_GetNightVisibility() > 0, "mounted goggles are lowered and powered after dusk");
        Check(Math.AbsFloat(night - 800) < 0.5, "working goggles reach the 800 m night sight cap, measured " + night);
        if (goggles) goggles.Expansion_TryTurningOff();
        Observer.Expansion_UpdateVisibility();
        Check(Observer.eAI_GetVisibilityDistanceLimit(Target) < 300, "switched-off goggles restore native unassisted night vision");
        GetGame().GetWorld().SetDate(year, month, day, hour, minute);
        Observer.eAI_SetThreatDistanceLimit(previousLimit);
        Observer.Expansion_UpdateVisibility(true);
        Observer.DisableSimulation(true);
        Print("[PredateTacticalTest] NVG SIGHT RANGE: " + night);
        Print("[PredateTacticalTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
    }

    bool WallBlocks(Object wall, vector start, vector finish, Object observer)
    {
        set<Object> hits = new set<Object>;
        vector contact, normal;
        int component;
        DayZPhysics.RaycastRV(start, finish, contact, normal, component, hits, null, observer, false, false, ObjIntersectView, 0);
        foreach (Object hit : hits)
            if (hit == wall) return true;
        return false;
    }

    float CheckHearing(eAIBase observer, PlayerBase target, vector heardAt, float now)
    {
        bool previousPreset = observer.m_PredateEliteAI;
        observer.m_PredateEliteAI = true;
        eAINoiseParams step = new eAINoiseParams("CfgVehicles SurvivorBase NoiseStepStand", eAINoiseType.SOUND);
        step.m_Strength = 200;
        int pending = observer.m_PredatePendingSounds.Count();
        eAINoiseSystem.AddNoiseEx(target, heardAt, step, 0.001);
        Check(observer.m_PredatePendingSounds.Count() == pending, "a footstep outside its audible radius produces no clue");
        eAINoiseSystem.AddNoiseEx(target, heardAt, step);
        Check(observer.m_PredatePendingSounds.Count() == pending + 1, "Expansion's footstep noise reaches the hearing queue");
        observer.PredateAdvanceInformation(now + 1);
        PredateObservation singleStep = observer.m_PredateMemory.Latest("", PredateObservationSource.FOOTSTEP, now + 1);
        Check(singleStep && singleStep.Confidence < 0.4 && singleStep.Radius >= 5 && singleStep.SubjectId == "", "one step is a weak anonymous clue");
        eAINoiseSystem.AddNoiseEx(target, heardAt, step);
        observer.PredateAdvanceInformation(now + 2);
        PredateObservation repeatedStep = observer.m_PredateMemory.Latest("", PredateObservationSource.FOOTSTEP, now + 2);
        Check(repeatedStep && repeatedStep.Confidence >= 0.4 && observer.m_eAI_NoiseTargetInfo.GetSource() == null, "close repeated steps strengthen the area without revealing who made them");
        eAINoiseParams shot = new eAINoiseParams("CfgWeapons M4A1 NoiseShoot", eAINoiseType.SHOT);
        eAINoiseSystem.AddNoiseEx(target, heardAt, shot);
        observer.PredateAdvanceInformation(now + 3);
        PredateObservation report = observer.m_PredateMemory.Latest("", PredateObservationSource.AUDIBLE, now + 3);
        Check(report && report.Confidence > 0.4 && report.ExpiresAt - report.ObservedAt > 12, "an audible muzzle report stays actionable longer than a footstep");
        observer.m_PredateEliteAI = previousPreset;
        return now + 3;
    }

    void RunNative(PredateSession session)
    {
        eAIBase observer = session.m_HuntBot;
        Check(observer && observer.m_PredateKnowledgeEnabled, "candidate registers information policy only on its operator");
        if (!observer) return;
        vector center = session.PredateGround("4500 0 6500");
        string building;
        array<string> buildings = {"Land_Mil_Barracks6", "Land_Mil_Barracks5", "Land_House_2B02", "Land_Mil_Barracks4"};
        foreach (string name : buildings)
        {
            if (!GetGame().ConfigIsExisting("CfgVehicles " + name)) continue;
            building = name;
            break;
        }
        Check(building != "", "a real native solid-cover asset exists");
        if (building == "") return;
        Object wall = GetGame().CreateObjectEx(building, center, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
        Check(wall != null, "native cover object created in validation world");
        eAIGroup targetGroup = eAIGroup.CreateGroup(new eAIFactionRaiders);
        Object created = GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", session.PredateGround(center + "1000 0 1000"), ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
        eAIBase independentObserver = eAIBase.Cast(created);
        independentObserver.SetGroup(eAIGroup.CreateGroup(new eAIFactionRaiders));
        independentObserver.DisableSimulation(true);
        PlayerBase target;
        array<Man> participants = new array<Man>;
        GetGame().GetPlayers(participants);
        foreach (Man participant : participants)
        {
            PlayerBase human = PlayerBase.Cast(participant);
            if (human && human.GetIdentity()) { target = human; break; }
        }
        if (!target)
        {
            Object body = GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", center + "40 0 0", ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN);
            target = PlayerBase.Cast(body);
        }
        Check(target != null, "static native character created");
        if (!target) return;
        if (target.GetIdentity())
        {
            vector observerPose = observer.GetBonePositionWS(observer.GetBoneIndexByName("neck")) - observer.GetPosition();
            vector targetPose = target.GetBonePositionWS(target.GetBoneIndexByName("neck")) - target.GetPosition();
            Check(observerPose[1] > 0.7 && targetPose[1] > 0.7, "selected actors have initialized native poses before the fixture freezes them");
        }
        target.SetGroup(targetGroup);
        targetGroup.m_BaseName = "KnowledgeValidationForeign";
        target.DisableSimulation(true);
        observer.DisableSimulation(true);
        vector start = session.PredateGround(center + "-40 0 0");
        observer.SetPosition(start);
        observer.SetOrientation("90 0 0");
        eAITargetInformation info = target.GetTargetInformation();
        bool newlyCreated;
        eAITarget state = info.AddAI(observer, 120000, true, newlyCreated);
        Check(state != null, "per-observer native target state exists");
        if (!state) return;
        state.m_LOS = false;
        vector known = session.PredateGround(center + "10 0 30");
        state.SetInitial(0.8, known);
        bool blockedPair;
        vector hiddenA, hiddenB;
        for (int offset = 2; offset <= 12; offset += 2)
        {
            hiddenA = session.PredateGround(center + Vector(40, 0, offset));
            hiddenB = session.PredateGround(center + Vector(40, 0, -offset));
            if (WallBlocks(wall, start + "0 1.5 0", hiddenA + "0 1.5 0", observer) && WallBlocks(wall, start + "0 1.5 0", hiddenB + "0 1.5 0", observer))
            {
                blockedPair = true;
                break;
            }
        }
        Check(blockedPair, "both alternative routes are behind actual native view geometry");
        target.SetPosition(hiddenA);
        vector answerA = state.GetPosition(true);
        float threatA = info.CalculateThreat(observer, state);
        target.SetPosition(hiddenB);
        target.SetAllowDamage(true);
        target.SetHealth("", "Health", 7);
        Check(Math.AbsFloat(target.GetHealth("", "Health") - 7) < 0.01, "occluded injury fixture actually changes native health");
        target.SetAllowDamage(false);
        vector answerB = state.GetPosition(true);
        float threatB = info.CalculateThreat(observer, state);
        Check(answerA == known && answerB == known, "two hidden routes return the same previous observation");
        Check(threatA == threatB, "unseen movement and injury do not update tactical threat");
        Check(state.m_LastKnownPosition == known, "hidden queries never refresh last-known position");
        state.m_LOS = true;
        state.UpdatePosition(true);
        state.UpdateThreat(true);
        Check(state.m_LastKnownPosition == known && info.CalculateThreat(observer, state) == threatA, "a stale positive LOS flag cannot refresh hidden position or live threat");
        state.m_LOS = false;
        vector aim = info.GetAimOffset(observer);
        Check(aim == state.m_PredateObservedAimOffset, "hidden posture cannot refresh aim offset");
        array<ref eAITarget> aimingTargets = observer.GetTargets();
        int aimingIndex = aimingTargets.Find(state);
        if (aimingIndex < 0) aimingTargets.InsertAt(state, 0);
        else if (aimingIndex > 0)
        {
            eAITarget firstAimTarget = aimingTargets[0];
            aimingTargets[0] = state;
            aimingTargets[aimingIndex] = firstAimTarget;
        }
        eAIAimingProfile aiming = new eAIAimingProfile(observer);
        state.m_PredateObservedVelocity = "1 0 0";
        target.SetPosition(hiddenA);
        aiming.Update();
        vector hiddenAimA = aiming.GetAimDirection();
        target.SetPosition(hiddenB);
        state.m_LOS = true;
        aiming.Update();
        vector hiddenAimB = aiming.GetAimDirection();
        Check(!state.m_LOS && state.m_LastKnownPosition == known, "aiming rechecks a stale positive visual flag against actual hard cover");
        Check(hiddenAimA == hiddenAimB && observer.m_PredateSensorDepth == 0, "native aiming does not follow either unseen route or retain sensor permission");
        Check(state.m_PredateObservedVelocity == "1 0 0", "unseen relocation cannot refresh the previously observed velocity");
        observer.m_PredateSensorDepth++;
        vector sensorTruth = info.GetPosition(observer, true, state);
        observer.m_PredateSensorDepth--;
        Check(sensorTruth == hiddenB && sensorTruth != known, "only the perception scope may query live geometry");
        observer.m_PredateKnowledgeEnabled = false;
        vector nativePosition = info.GetPosition(observer, true, state);
        observer.m_PredateKnowledgeEnabled = true;
        Check(nativePosition == hiddenB, "unregistered/native AI behavior remains unchanged");
        observer.m_PredateKnowledgeEnabled = false;
        state.m_LastKnownPosition = hiddenB;
        observer.PredateBindInformation(true, "raven-01", observer.m_PredateMemory, session.m_WorldState.PendingSounds, observer.m_PredateSimulationTime);
        Check(state.m_LastKnownPosition != hiddenB && !state.m_PredateHasPosition, "entity replacement discards coordinates acquired before its policy was bound");
        Check(state.m_PredateObservedVelocity == vector.Zero && observer.m_PredateLastVisualAt < 0, "rebinding clears old motion and visual reaction permission");
        state.SetInitial(0.8, known);
        state.m_LOS = false;
        state.UpdatePosition(true);
        Check(state.m_LastKnownPosition == known, "forced refresh does not bypass occlusion knowledge");
        eAITargetInformationState independent = independentObserver.eAI_GetTargetInformationState(info, false);
        Check(independent != state && independent.m_LastKnownPosition != known, "native target caches remain per observer");
        state.m_PredateObservedRaised = true;
        state.m_PredateObservedUnconscious = true;
        Check(state.IsRaised() && state.IsUnconscious(), "occluded posture and injury state come from the observation snapshot");
        Check(!state.CanMeleeIfClose(), "hidden live health is not queried to choose melee");
        bool moving, friendly, targeted;
        Check(observer.PlayerIsEnemy(target, false, moving, friendly, targeted) && !moving && !friendly, "independent enemy classification does not read hidden movement or another group's threat memory");

        state.m_LOS = false;
        eAINoiseParams noise = new eAINoiseParams("CfgWeapons M4A1 NoiseShoot", eAINoiseType.BULLETIMPACT);
        noise.m_Strength = 200;
        observer.eAI_OnNoiseEvent(target, hiddenA, 120, noise, 1);
        Check(observer.m_PredatePendingSounds.Count() > 0, "qualifying audible impact queues a delayed observation");
        float soundTime = observer.m_PredateSimulationTime + 2;
        observer.PredateAdvanceInformation(soundTime);
        Check(observer.m_eAI_NoiseTargetInfo.GetSource() == null, "native noise investigation receives no shooter entity");
        session.m_WorldState.SimulationSeconds = soundTime;
        PredateObservation sound = observer.m_PredateMemory.Latest("", PredateObservationSource.IMPACT, soundTime);
        Check(sound && sound.SubjectId == "" && sound.Radius >= 5, "heard impact remains an uncertain unidentified event");
        Check(!observer.m_PredateMemory.Latest("contact-a", PredateObservationSource.IMPACT, soundTime), "a nearby impact does not identify the selected signature");
        session.m_WorldState.SimulationSeconds = CheckHearing(observer, target, hiddenA, soundTime);
        Session = session;
        Observer = observer;
        Target = target;
        State = state;
        observer.SetAllowDamage(false);
        target.SetAllowDamage(true);
        target.SetHealth("", "Health", 100);
        target.SetHealth("", "Blood", 5000);
        target.SetHealth("", "Shock", 100);
        target.SetAllowDamage(false);
        observer.DisableSimulation(false);
        target.DisableSimulation(false);
        observer.GetGroup().SetWaypointBehaviour(eAIWaypointBehavior.ROAMING_LOCAL);
        target.GetGroup().SetWaypointBehaviour(eAIWaypointBehavior.ROAMING_LOCAL);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PrepareVisible, 1500, false);
    }

    void PrepareVisible()
    {
        vector look = Observer.GetLookDirection();
        look[1] = 0;
        look.Normalize();
        float heading = PredateMath.CameraHeading(look);
        if (VisibleAttempts % 3 == 1) heading += 20;
        else if (VisibleAttempts % 3 == 2) heading -= 20;
        vector direction = PredateInformationMath.BearingDirection(heading);
        float range = 10;
        if (VisibleAttempts >= 3) range = 5;
        vector visiblePosition = Session.PredateGround(Observer.GetPosition() + direction * range);
        Target.SetPosition(visiblePosition);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckVisible, 300, false);
    }

    void CheckVisible()
    {
        array<ref eAITarget> candidates = Observer.GetTargets();
        int selected = candidates.Find(State);
        if (selected > 0)
        {
            eAITarget previous = candidates[0];
            candidates[0] = State;
            candidates[selected] = previous;
        }
        bool targetSelected = Observer.GetTarget() == State;
        Observer.m_eAI_TargetChanged = true;
        Observer.m_eAI_LOSCheckDT = 1;
        bool reacquired = Observer.EnforceLOS(1.0);
        if (!reacquired)
        {
            Observer.m_PredateKnowledgeEnabled = false;
            Observer.m_eAI_TargetChanged = true;
            Observer.m_eAI_LOSCheckDT = 1;
            bool controlVisible = Observer.EnforceLOS(1.0);
            Observer.m_PredateKnowledgeEnabled = true;
            Print("[PredateTacticalTest] Sensor control: attempt=" + VisibleAttempts + "; native=" + controlVisible + "; target=" + Target.GetPosition());
            // Hard cover or a changing native animation must not masquerade as
            // an adapter failure. Retry only when the native control also fails.
            if (!controlVisible && VisibleAttempts < 5)
            {
                VisibleAttempts++;
                GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PrepareVisible, 500, false);
                return;
            }
        }
        Check(targetSelected, "visual probe selects the character rather than the earlier noise");
        Check(reacquired, "native facing/occlusion sensor reacquires a visible character");
        if (reacquired)
        {
            State.UpdatePosition(false);
            vector actual = Target.GetPosition();
            Check(vector.Distance(State.m_LastKnownPosition, actual) < 0.1, "fresh visual observation updates the cached location");
            CheckEliteRecognition();
        }
        else
        {
            Observer.m_PredateKnowledgeEnabled = false;
            Observer.m_eAI_TargetChanged = true;
            Observer.m_eAI_LOSCheckDT = 1;
            bool nativeVisible = Observer.EnforceLOS(1.0);
            Observer.m_PredateKnowledgeEnabled = true;
            Print("[PredateTacticalTest] Sensor diagnostic: native=" + nativeVisible + "; target=" + Target.GetPosition() + "; eyes=" + Observer.GetBonePositionWS(Observer.GetBoneIndexByName("head")));
        }
        Target.DisableSimulation(true);
        CheckSightRange();
    }

    void CheckEliteRecognition()
    {
        bool previousPolicy = Observer.m_PredateEliteAI;
        float now = Observer.m_PredateSimulationTime;
        State.m_PredateRecognition = null;
        Observer.m_PredateEliteAI = true;
        bool first = Observer.PredateRecognize(State, Target, 4);
        bool duplicate = Observer.PredateRecognize(State, Target, 4);
        PredateObservation clue = Observer.m_PredateMemory.Latest("", PredateObservationSource.GLIMPSE, now);
        Check(!first && !duplicate && clue && clue.SubjectId == "" && clue.VisualContactKey == "" && clue.Radius >= 12, "the first glimpse yields an anonymous area regardless of repeated sensor callers");
        Observer.m_PredateSimulationTime = now + 0.25;
        Check(Observer.PredateRecognize(State, Target, 4), "a sustained second observation confirms the contact");
        Check(!Observer.PredateRecognize(State, Target, 0), "occlusion denies visual permission in the same simulation tick");
        Observer.m_PredateSimulationTime = now;
        Observer.m_PredateEliteAI = previousPolicy;
        State.m_PredateRecognition = null;
    }
}
