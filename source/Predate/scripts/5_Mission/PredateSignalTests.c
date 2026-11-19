class PredateSignalTests
{
    int Checks;
    int Failures;

    void Check(bool passed, string label)
    {
        Checks++;
        if (passed) return;
        Failures++;
        Print("[PredateSignalTest] FAIL: " + label);
    }

    void Run()
    {
        PredateConfig config = new PredateConfig;
        PredateSignalTrack human = PredateSignalService.Track("player-01", "raven-01");
        PredateSignalTrack ai = PredateSignalService.Track("raven-01", "player-01");
        PredateSignalTrack sliding = PredateSignalService.Track("player-01", "lynx-01");
        sliding.UseBearingSectors(1);
        PredateObservation firstPulse = PredateSignalRules.ReceiverReading(sliding, "0 0 0", "0 300 500", 1, config);
        sliding.RecordSample(firstPulse);
        PredateObservation nextPulse = PredateSignalRules.ReceiverReading(sliding, "0 0 0", "0 300 500", 91, config);
        Check(firstPulse && nextPulse && firstPulse.Bearing != nextPulse.Bearing && PredateBearingPulse.Contains(firstPulse.Bearing, 0) && PredateBearingPulse.Contains(nextPulse.Bearing, 0), "successive bearings vary for a stationary target while covering north");
        Check(sliding.LastSample && Math.AbsFloat(sliding.LastSample.PulseDistance - 500) < 0.05 && sliding.History[0].PulseDistance == sliding.LastSample.PulseDistance, "horizontal range is frozen with the sampled bearing");
        Check(PredateBearingPulse.Overlap(355, 6) && PredateBearingPulse.Overlap(6, 355) && !PredateBearingPulse.Overlap(355, 10), "partial overlaps wrap through north");
        sliding.PlayerPulseFormat = 1;
        sliding.NextPulseAt = 91;
        sliding.UseBearingSectors(2);
        Check(sliding.PlayerPulseFormat == 2 && !sliding.LastSample && sliding.NextPulseAt == 91 && sliding.History.Count() == 1, "legacy range migration preserves cooldown and history");
        Check(!human.Due(0), "an unselected signature provides no pulse");
        human.SetSelected(true, 10, 60);
        ai.SetSelected(true, 10, 60);
        Check(human.Due(10) && ai.Due(10), "first lock is immediate for both receivers");
        PredateObservation h = PredateSignalRules.Sample(human, "0 0 0", "0 0 500", 10, config, 0, 1);
        PredateObservation a = PredateSignalRules.Sample(ai, "0 0 500", "0 0 0", 10, config, 0, 1);
        Check(h && a && h.Valid("player-01", 10) && a.Valid("raven-01", 10), "both samples use valid owned observations");
        Check(h.Bearing == 3 && a.Bearing == 183 && h.BearingError == a.BearingError, "equal sampling error and reciprocal headings");
        Check(!h.HasPosition && h.Estimate == vector.Zero && h.Origin == "0 0 0", "sample carries historical observer origin without target position");
        Check(h.MapRadius == config.FinalApproachRadius && h.MapCenter != "0 0 500", "human map circle uses the visual approach radius and a coarse center");
        Check(PredateSignalRules.WithinFinalApproach(h.MapCenter, "0 0 500", h.MapRadius), "circle covers the sampled point even at a cell corner");
        Check(h.MapCenter == PredatePulseArea.CoarseCenter("100 20 560", h.MapRadius), "positions in one cell cannot be distinguished from repeated map areas");
        Check(a.MapRadius == 0 && a.MapCenter == vector.Zero, "AI keeps bearing-only evidence");
        Check(human.Consume(10, 60, h) && ai.Consume(10, 60, a), "both scheduled samples consumed");
        h.Bearing = 99;
        Check(human.LastSample.Bearing == 3 && human.History[0].Bearing == 3, "track stores independent sample copies");
        human.SetSelected(false, 20, 60);
        human.SetSelected(true, 30, 60);
        Check(human.NextPulseAt == 70 && !human.Due(30), "quick clear and reselect cannot reset cooldown");
        human.SetSelected(false, 40, 60);
        human.SetSelected(true, 195, 60);
        Check(human.NextPulseAt == 250 && !human.Due(195), "unlocked elapsed slots are skipped without fresh information");
        Check(human.LastSample.ObservedAt == 10 && human.LastSample.Expired(195), "switching does not refresh stale sample age");
        Check(!human.Consume(250, 0, null) && human.NextPulseAt == 250, "invalid interval cannot corrupt schedule");
        h = PredateSignalRules.Sample(human, "100 0 0", "100 0 199", 250, config, 1, 0);
        Check(PredateSignalRules.WithinFinalApproach("100 0 0", "100 300 199", config.FinalApproachRadius) && !PredateSignalRules.WithinFinalApproach("100 0 0", "100 300 200", config.FinalApproachRadius), "elevation cannot change the horizontal final-approach boundary");
        Check(!h && human.Consume(250, 60, h), "close approach suppresses and consumes a scheduled slot");
        Check(!human.LastSample && !human.Due(251) && human.NextPulseAt == 310, "leaving close approach grants no unscheduled sample");
        h = PredateSignalRules.Sample(human, "100 0 0", "100 0 200", 310, config, 1, 1);
        Check(h && h.BearingError == 2.5 && h.Bearing == 2, "cutoff boundary and quantized stronger-signal uncertainty");
        Check(human.Consume(310, 60, h), "next scheduled sample resumes assistance");
        Check(!human.Consume(310, 60, h), "same sample cannot be consumed twice");
        a = PredateSignalRules.Sample(ai, "0 0 500", "0 0 0", 195, config, 0, 0);
        Check(ai.Consume(195, 60, a) && ai.NextPulseAt == 250 && ai.NextSampleSerial == 3, "late tick emits one sample and retains phase without catch-up burst");
        PredateSignatureActivity energy = new PredateSignatureActivity;
        energy.RecordShots(1);
        Check(energy.Energy == 0.25, "confirmed shot increases activity");
        energy.RecordShots(200);
        Check(energy.Energy == 1, "activity is bounded under bursts");
        energy.Advance(100);
        Check(Math.AbsFloat(energy.Energy - 0.5) < 0.0001, "activity decays with active simulation time");
        energy.Advance(0);
        Check(Math.AbsFloat(energy.Energy - 0.5) < 0.0001, "pause supplies no activity decay");
        energy.Advance(1000);
        Check(energy.Energy == 0, "decay is bounded at zero");
        energy.RecordExplosions(1);
        Check(energy.Energy == 0.5, "one owned blast increases next-sample activity");
        energy.RecordExplosions(99);
        Check(energy.Energy == 1, "blast activity is bounded");
        energy.Advance(1000);
        energy.RecordEngineUse(5);
        Check(Math.AbsFloat(energy.Energy - 0.1) < 0.0001, "driver engine activity accumulates with active elapsed time");
        energy.RecordEngineUse(0);
        energy.RecordEngineUse(-1);
        Check(Math.AbsFloat(energy.Energy - 0.1) < 0.0001, "paused or invalid elapsed time cannot increase engine activity");
        PredateState display = new PredateState;
        display.Available = true;
        display.Locked = true;
        display.HasPulse = true;
        display.HasPulseOrigin = true;
        display.PulseOrigin = "4500 100 6500";
        Check(PredateReceiverText.Origin(display, "4530 200 6540") == "Sample origin E 4500 / N 6500 | 50 m from you", "receiver origin reports horizontal travel from the historical sample");
        display.CloseApproach = true;
        Check(PredateReceiverText.Origin(display, "4530 200 6540") == "Bearings freeze between pulses.", "close approach hides historical-origin guidance");
        display.CloseApproach = false;
        display.Locked = false;
        Check(PredateReceiverText.Origin(display, "4530 200 6540") == "Bearings freeze between pulses.", "clearing a lock hides sample-origin guidance");
        display.Locked = true;
        display.HasPulseOrigin = false;
        Check(PredateReceiverText.Origin(display, "4530 200 6540") == "This older sample has no recorded origin.", "legacy samples cannot invent a historical origin");
        Check(PredateProtocol.AcceptRequest(PredateProtocol.VERSION, "raven-01", 1) && !PredateProtocol.AcceptRequest(PredateProtocol.VERSION - 1, "raven-01", 1), "receiver packet revision rejects the previous incompatible protocol");
        for (int i = 0; i < 8; i++)
        {
            float time = 370 + i * 60;
            h = PredateSignalRules.Sample(human, Vector(i * 40, 0, 0), "0 0 800", time, config, 0, 0);
            human.Consume(time, 60, h);
        }
        Check(human.History.Count() == 4 && human.Validate(790), "bounded historical ray archive remains valid");
        Check(human.History[0].ObservedAt == 610 && human.History[3].ObservedAt == 790, "history retains the most recent four chronologically");
        string error;
        Check(JsonFileLoader<PredateSignalTrack>.SaveFile("$profile:SignalRoundTrip.json", human, error), "native signal schedule save");
        PredateSignalTrack restored = new PredateSignalTrack;
        Check(JsonFileLoader<PredateSignalTrack>.LoadFile("$profile:SignalRoundTrip.json", restored, error) && restored.Validate(790), "native schedule reload validates");
        Check(restored.NextPulseAt == human.NextPulseAt && restored.NextSampleSerial == human.NextSampleSerial && restored.LastSample.Origin == human.LastSample.Origin && restored.LastSample.ObservedAt == human.LastSample.ObservedAt, "reload preserves phase, serial, origin and sample time");
        Check(restored.LastSample.MapCenter == human.LastSample.MapCenter && restored.LastSample.MapRadius == human.LastSample.MapRadius, "cold restoration preserves the coarse area");
        restored.History.InsertAt(restored.History[3], 0);
        restored.History.RemoveOrdered(4);
        restored.NormalizeLoaded();
        Check(restored.History[0].ObservedAt == 610 && restored.History[3].ObservedAt == 790, "legacy out-of-order samples restore chronologically");
        restored.LastSample.ObserverId = "somebody-else";
        Check(!restored.Validate(790), "cross-owner stored information rejected");
        PredateSignalTrack empty = PredateSignalService.Track("player-01", "raven-01");
        JsonFileLoader<PredateSignalTrack>.SaveFile("$profile:EmptySignalRoundTrip.json", empty, error);
        restored = new PredateSignalTrack;
        JsonFileLoader<PredateSignalTrack>.LoadFile("$profile:EmptySignalRoundTrip.json", restored, error);
        restored.NormalizeLoaded();
        Check(!restored.LastSample && restored.Validate(790), "native empty-reference representation reloads as no sample");
        CheckPrivateHunt();
        Print("[PredateSignalTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
    }

    void CheckPrivateHunt()
    {
        PredateConfig config = new PredateConfig;
        PredateHuntReceiver receiver = new PredateHuntReceiver;
        receiver.OwnerId = "raven-01";
        PredateHunterState hunter = new PredateHunterState;
        hunter.OwnerId = receiver.OwnerId;
        PredateObservationMemory memory = new PredateObservationMemory;
        memory.OwnerId = receiver.OwnerId;
        PredateSignalTrack near = PredateSignalService.Track(receiver.OwnerId, "lynx-01");
        PredateSignalTrack far = PredateSignalService.Track(receiver.OwnerId, "player-01");
        near.RecordSample(PredateSignalRules.ReceiverReading(near, vector.Zero, "0 0 400", 10, config));
        far.RecordSample(PredateSignalRules.ReceiverReading(far, vector.Zero, "0 0 1800", 10, config));
        receiver.Tracks.Insert(near);
        receiver.Tracks.Insert(far);
        PredateHuntSelection.Update(receiver, hunter, vector.Zero, 10, false);
        Check(receiver.TargetId == "lynx-01", "same receiver prefers a nearer AI over a farther human");
        Check(!PredateSignalRules.ReceiverReading(near, vector.Zero, "0 0 199", 10, config) && !PredateSignalRules.ReceiverReading(far, vector.Zero, "0 0 5001", 10, config), "private receiver uses the player cutoff and outer range");
        near.RecordSample(PredateSignalRules.ReceiverReading(near, vector.Zero, "0 0 400", 100, config));
        far.RecordSample(PredateSignalRules.ReceiverReading(far, vector.Zero, "0 0 1800", 100, config));
        PredateHuntSelection.Update(receiver, hunter, vector.Zero, 100, false);
        Check(receiver.TargetId == "lynx-01" && receiver.Replanned, "one stalled window tries a new approach");
        PredateHuntSelection.Update(receiver, hunter, vector.Zero, 190, false);
        Check(receiver.TargetId == "player-01" && receiver.DeferredId == "lynx-01", "two stalled windows choose the closest alternative");
        PredateHuntSelection.Update(receiver, hunter, vector.Zero, 191, false);
        Check(receiver.TargetId == "player-01", "cooldown prevents an immediate return to the stalled target");
        Check(receiver.Valid(191, "raven-01") && !receiver.Valid(191, "rook-01"), "private hunt persistence validates ownership");
        receiver.Retire("player-01");
        PredateObservation oldContact = new PredateObservation;
        oldContact.Source = PredateObservationSource.VISUAL;
        oldContact.SubjectId = "fixture-player";
        oldContact.ObservedAt = 191;
        oldContact.ExpiresAt = 311;
        oldContact.HasPosition = true;
        oldContact.Estimate = "0 0 15";
        memory.Entries.Insert(oldContact);
        memory.AnonymizeSightings();
        PredateHuntSelection.Update(receiver, hunter, vector.Zero, 192, false);
        Check(oldContact.SubjectId == "" && oldContact.VisualContactKey != "" && receiver.TargetId != "player-01", "a visual contact cannot select a radio call sign");
    }

    void RunService(PredateSession session, PlayerBase player)
    {
        eAIBase bot = session.m_HuntBot;
        player.SetAllowDamage(false);
        bot.SetAllowDamage(false);
        bot.DisableSimulation(true);
        vector humanPosition = "4500 0 6500";
        vector botPosition = "4500 0 6900";
        humanPosition[1] = GetGame().SurfaceY(4500, 6500);
        botPosition[1] = GetGame().SurfaceY(4500, 6900);
        player.SetPosition(humanPosition);
        bot.SetPosition(botPosition);
        PredateSignalService.NewDeployment(session);
        float now = session.m_WorldState.SimulationSeconds;
        PredateSignalService.Select(session, true);
        PredateSignalService.Tick(session, player, 0);
        PredateWorldState state = session.m_WorldState;
        state.Hunter.OwnerId = "raven-01";
        PredateHuntRuntime.Prepare(state.HuntReceiver, state.Hunter, "raven-01", now);
        PredateHuntParticipant observer = new PredateHuntParticipant("raven-01", bot, state.HuntReceiver);
        array<ref PredateHuntParticipant> participants = {new PredateHuntParticipant("player-01", player), observer};
        PredateHuntRuntime.Scan(observer, participants, session.m_HuntConfig, now);
        PredateSignalTrack privateTrack = state.HuntReceiver.Find("player-01");
        Check(state.PlayerSignal.LastSample && privateTrack && privateTrack.LastSample, "live actors receive reciprocal scheduled pulses");
        Check(state.PlayerSignal.LastSample.Origin == humanPosition && privateTrack.LastSample.Origin == botPosition, "live service captures each recipient origin");
        Check(privateTrack.LastSample.DistanceBand == 2 && privateTrack.LastSample.PulseDistance == 0, "AI receives the player-visible distance band");
        PredateSignalService.MarkPlayerHunted(session, now);
        Check(state.PlayerInterceptionUntil == now + PredateBearingPulse.INTERVAL + 30 && state.BotInterceptionUntil == state.PlayerInterceptionUntil, "interceptions retain the shared pulse cadence");
        int serial = privateTrack.NextSampleSerial;
        vector displaced = botPosition + "400 0 0";
        bot.SetPosition(displaced);
        PredateSignalService.Tick(session, player, 0);
        PredateHuntRuntime.Scan(observer, participants, session.m_HuntConfig, now);
        Check(privateTrack.NextSampleSerial == serial && privateTrack.LastSample.Origin == botPosition, "ordinary ticks cannot refresh historical origin");
        PredateSignalTrack scheduled = PredateSignalService.Track("player-01", "raven-01");
        PredateSignalService.SamplePlayerPulse(scheduled, player, bot, now, session.m_HuntConfig);
        bot.SetPosition(humanPosition + "0 0 50");
        Check(!PredateSignalService.SamplePlayerPulse(scheduled, player, bot, now + 1, session.m_HuntConfig) && scheduled.Reception == PredatePulseReception.BEARING && scheduled.LastSample, "crossing the cutoff between polls preserves the previous display");
        Check(PredateSignalService.SamplePlayerPulse(scheduled, player, bot, now + 90, session.m_HuntConfig) && scheduled.Reception == PredatePulseReception.CLOSE && !scheduled.LastSample && scheduled.History.Count() == 1, "scheduled cutoff retains historical evidence");
        Check(!PredateSignalService.SamplePlayerPulse(scheduled, player, null, now + 91, session.m_HuntConfig) && scheduled.Reception == PredatePulseReception.CLOSE, "vanishing contact does not leak between scheduled polls");
        Check(PredateSignalService.SamplePlayerPulse(scheduled, player, null, now + 180, session.m_HuntConfig) && scheduled.Reception == PredatePulseReception.UNAVAILABLE && scheduled.History.Count() == 1, "the next poll confirms signal loss");
        bot.SetPosition(displaced);
        PredateHunterState scout = new PredateHunterState;
        PredateHuntRuntime.Scout(scout, botPosition, now);
        Check(scout.HasGoal && scout.Mode == "scout" && PredateHunterBrain.Distance(botPosition, scout.Goal) > 90, "quiet receivers can request a traversable scouting leg without a target");
        Check(PredateBearingPulse.Contains(state.PlayerSignal.LastSample.Bearing, 0) && state.PlayerSignal.LastSample.MapRadius == 0 && Math.AbsFloat(state.PlayerSignal.LastSample.PulseDistance - 400) < 0.05, "player pulse keeps its sampled bearing and horizontal distance without target position");
        Check(state.PlayerSignal.BroadcastTime == state.PlayerSignal.LastSample.ObservedAt, "map and compass read one pulse");
        Check(Math.AbsFloat(state.PlayerSignal.NextPulseAt - state.PlayerSignal.BroadcastTime - 90) < 0.005, "both displays wait 90 seconds for the next pulse");
        Check(!PredateSignalRules.ReceiverReading(state.PlayerSignal, "0 0 0", "0 0 5001", now, session.m_HuntConfig), "a rival outside five kilometres does not broadcast a sector");
        PredateObservation boundary = PredateSignalRules.ReceiverReading(state.PlayerSignal, "0 0 0", "0 300 5000", now, session.m_HuntConfig);
        Check(boundary && Math.AbsFloat(boundary.PulseDistance - 5000) < 0.05 && PredateBearingPulse.Contains(boundary.Bearing, 0), "horizontal five kilometre boundary is included");
        PredateSignalService.Select(session, false);
        PredateSignalService.Select(session, true);
        Check(!state.PlayerSignal.Due(now), "live request clear/reselect retains cooldown");
        Weapon_Base weapon = Weapon_Base.Cast(bot.GetHumanInventory().GetEntityInHands());
        Check(weapon != null, "shot-event fixture has a firearm");
        if (weapon)
        {
            int muzzle = weapon.GetCurrentMuzzle();
            if (weapon.IsChamberEmpty(muzzle)) weapon.PushCartridgeToChamber(muzzle, 0, "Bullet_556x45");
            int shots = bot.m_PredatePendingShots;
            vector muzzlePosition = displaced + "0 1.5 0";
            bool fired = weapon.Fire(muzzle, muzzlePosition, "0 1 0", "0 1 0");
            Check(fired && bot.m_PredatePendingShots == shots + 1, "actual server firearm event counted exactly once");
            PredateSignalService.Capture(session);
            Check(state.BotActivity.Energy > 0 && bot.m_PredatePendingShots == 0, "save capture consumes confirmed firearm activity");
            float energy = state.BotActivity.Energy;
            PredateSignalService.Capture(session);
            Check(state.BotActivity.Energy == energy, "repeated saves do not count the same shot twice");
        }
        RunBlastService(session, player);
    }

    void FinishService(PredateSession session)
    {
        PredateSupplyTests supplyTests = new PredateSupplyTests;
        supplyTests.Run(session);
        PredateRivalTests rivalTests = new PredateRivalTests;
        rivalTests.Run(session);
        session.PredateSave();
        Print("[PredateSignalServiceTest] COMPLETE: checks=" + Checks + "; failures=" + Failures);
    }

    void RunBlastService(PredateSession session, PlayerBase player)
    {
        eAIBase bot = session.m_HuntBot;
        Grenade_Base grenade = Grenade_Base.Cast(bot.GetInventory().CreateInInventory("M67Grenade"));
        Check(grenade != null, "native owned fragmentation device can be created");
        if (!grenade) { FinishService(session); return; }
        Check(grenade.m_PredateExplosionOwner == bot, "inventory ownership records the registered operator");
        grenade.Unpin();
        Check(bot.m_PredatePendingExplosions == 0, "arming alone is not an explosion event");
        bool handed = player.GetInventory().TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.ANY_CARGO, grenade);
        Check(handed, "native inventory handoff request accepted");
        if (!handed) { FinishService(session); return; }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AfterBlastHandoff, 500, false, session, player, grenade);
    }

    void AfterBlastHandoff(PredateSession session, PlayerBase player, Grenade_Base grenade)
    {
        Check(grenade.GetHierarchyRootPlayer() == player && grenade.m_PredateExplosionOwner == player, "a completed inventory handoff changes blast attribution");
        bool dropped = player.GetInventory().DropEntity(InventoryMode.SERVER, player, grenade);
        Check(dropped, "native grenade drop request accepted");
        if (!dropped) { FinishService(session); return; }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AfterBlastDrop, 500, false, session, player, grenade);
    }

    void AfterBlastDrop(PredateSession session, PlayerBase player, Grenade_Base grenade)
    {
        eAIBase bot = session.m_HuntBot;
        bool dropped = !grenade.GetHierarchyRootPlayer();
        Check(dropped && !grenade.GetHierarchyRootPlayer(), "native grenade is detached before the remote blast fixture");
        if (!dropped) { FinishService(session); return; }
        vector blastPosition = player.GetPosition() + "900 0 900";
        blastPosition[1] = GetGame().SurfaceY(blastPosition[0], blastPosition[2]);
        grenade.SetPosition(blastPosition);
        int before = player.m_PredatePendingExplosions;
        int botBefore = bot.m_PredatePendingExplosions;
        float serial = session.m_WorldState.HuntReceiver.NextPulseAt;
        grenade.ActivateImmediate();
        Check(player.m_PredatePendingExplosions == before + 1 && bot.m_PredatePendingExplosions == botBefore, "actual blast credits its latest known holder exactly once");
        PredateSignalService.Capture(session);
        float energy = session.m_WorldState.PlayerActivity.Energy;
        Check(energy >= 0.5 && player.m_PredatePendingExplosions == 0, "save capture consumes the confirmed blast");
        PredateSignalService.Capture(session);
        Check(session.m_WorldState.PlayerActivity.Energy == energy && session.m_WorldState.HuntReceiver.NextPulseAt == serial, "repeated captures neither duplicate a blast nor grant a fresh pulse");
        Grenade_Base unknown = Grenade_Base.Cast(GetGame().CreateObjectEx("M67Grenade", blastPosition + "10 0 0", ECE_CREATEPHYSICS | ECE_KEEPHEIGHT));
        Check(unknown != null, "unattributed native blast fixture exists");
        if (!unknown) { FinishService(session); return; }
        unknown.ActivateImmediate();
        Check(player.m_PredatePendingExplosions == 0 && bot.m_PredatePendingExplosions == botBefore, "an unowned explosion is not assigned to a nearby signature");
        RunEngineService(session, player);
    }

    void RunEngineService(PredateSession session, PlayerBase player)
    {
        vector position = player.GetPosition() + "15 0 0";
        position[1] = GetGame().SurfaceY(position[0], position[2]);
        CarScript car = CarScript.Cast(GetGame().CreateObjectEx("OffroadHatchback", position, ECE_CREATEPHYSICS | ECE_KEEPHEIGHT));
        Check(car != null, "native vehicle fixture exists");
        if (!car) { FinishService(session); return; }
        car.OnDebugSpawn();
        HumanCommandVehicle command = player.StartCommand_Vehicle(car, 0, car.GetSeatAnimationType(0));
        Check(command != null, "native driver command starts");
        if (!command) { FinishService(session); return; }
        command.SetVehicleType(car.GetAnimInstance());
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AfterEngineEntry, 4000, false, session, player, car);
    }

    void AfterEngineEntry(PredateSession session, PlayerBase player, CarScript car)
    {
        Check(car.CrewMemberIndex(player) == 0 && !PredateSignalService.OperatingEngine(player), "a seated driver with engine stopped creates no engine activity");
        car.EngineStart();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AfterEngineStart, 1000, false, session, player, car);
    }

    void AfterEngineStart(PredateSession session, PlayerBase player, CarScript car)
    {
        Check(car.EngineIsOn() && PredateSignalService.OperatingEngine(player), "a real running engine is attributed to its driver");
        float before = session.m_WorldState.PlayerActivity.Energy;
        PredateSignalService.Tick(session, player, 0);
        Check(session.m_WorldState.PlayerActivity.Energy == before, "zero simulation time adds no driver engine activity");
        session.m_WorldState.PlayerActivity.Energy = 0;
        float serial = session.m_WorldState.HuntReceiver.NextPulseAt;
        PredateSignalService.Tick(session, player, 5);
        Check(Math.AbsFloat(session.m_WorldState.PlayerActivity.Energy - 0.075) < 0.0001 && session.m_WorldState.HuntReceiver.NextPulseAt == serial, "actual driver activity grows with simulation time without an extra pulse");
        HumanCommandVehicle command = player.GetCommand_Vehicle();
        if (!command) { Check(false, "driver command remains active"); FinishService(session); return; }
        command.SwitchSeat(1, car.GetSeatAnimationType(1));
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(AfterPassengerSwitch, 4000, false, session, player, car);
    }

    void AfterPassengerSwitch(PredateSession session, PlayerBase player, CarScript car)
    {
        Check(car.CrewMemberIndex(player) == 1 && car.EngineIsOn() && !PredateSignalService.OperatingEngine(player), "a real passenger does not inherit running engine activity");
        float before = session.m_WorldState.PlayerActivity.Energy;
        PredateSignalService.Tick(session, player, 5);
        Check(Math.AbsFloat(session.m_WorldState.PlayerActivity.Energy - Math.Max(0, before - 0.025)) < 0.0001, "passenger tick only decays existing activity");
        car.EngineStop();
        Check(!PredateSignalService.OperatingEngine(player), "stopping the engine removes its activity source");
        HumanCommandVehicle command = player.GetCommand_Vehicle();
        if (command) command.GetOutVehicle();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FinishEngineService, 4000, false, session, player);
    }

    void FinishEngineService(PredateSession session, PlayerBase player)
    {
        Check(!player.GetCommand_Vehicle() && !PredateSignalService.OperatingEngine(player), "on-foot actor has no engine activity source");
        FinishService(session);
    }
}
