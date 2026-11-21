// Deliberately engine-free. Destinations depend only on private evidence and
// the operator's own pose/status; no target entity or live target position enters.
class PredateHunterState
{
    int Schema = 1;
    string OwnerId = "raven-01";
    string Mode = "hold";
    string Reason = "No observations";
    string EvidenceKey;
    string LastSignalKey;
    string LastContactKey;
    bool HasGoal;
    vector Goal;
    bool HasDetour;
    vector Detour;
    int GoalRevision;
    vector SearchCenter;
    float SearchRadius;
    float SearchObservedAt;
    float SearchUntil;
    int SearchStep;
    int CheckedSectors;
    int SearchWatchRevision = -1;
    float NextDecisionAt;
    vector ProgressPosition;
    float ProgressAt;
    int RouteFailures;
    bool InterceptionHandled;
    float ScoutAt;
    float ScoutHeading;
    vector WatchPoint;
    float WatchUntil;
    bool FinalApproach;

    bool Validate(float now, string expectedOwner = "raven-01")
    {
        if (Schema != 1 || OwnerId != expectedOwner || GoalRevision < 0 || SearchStep < 0 || RouteFailures < 0 || RouteFailures > 3) return false;
        if (CheckedSectors < 0 || CheckedSectors > 255 || SearchWatchRevision < -1 || SearchWatchRevision > GoalRevision) return false;
        if (!PredateInformationMath.FiniteVector(Goal) || !PredateInformationMath.FiniteVector(Detour) || !PredateInformationMath.FiniteVector(SearchCenter) || !PredateInformationMath.FiniteVector(ProgressPosition)) return false;
        if (HasDetour && (!HasGoal || RouteFailures == 0 || RouteFailures >= 3)) return false;
        if (!PredateInformationMath.Finite(NextDecisionAt) || NextDecisionAt < 0 || !PredateInformationMath.Finite(ProgressAt) || ProgressAt < 0 || ProgressAt > now) return false;
        if (!PredateInformationMath.Finite(SearchObservedAt) || SearchObservedAt < 0 || SearchObservedAt > now) return false;
        if (!PredateInformationMath.Finite(SearchRadius) || SearchRadius < 0 || SearchRadius > 1000 || !PredateInformationMath.Finite(SearchUntil) || SearchUntil < 0) return false;
        if (!PredateInformationMath.Finite(ScoutAt) || ScoutAt < 0 || !PredateInformationMath.Finite(ScoutHeading) || ScoutHeading < 0 || ScoutHeading >= 360) return false;
        if (!PredateInformationMath.FiniteVector(WatchPoint) || !PredateInformationMath.Finite(WatchUntil) || WatchUntil < 0) return false;
        return Mode == "hold" || Mode == "observe" || Mode == "evade" || Mode == "approach" || Mode == "search" || Mode == "investigate" || Mode == "combat" || Mode == "recover" || Mode == "reassess" || Mode == "scout";
    }

    float SearchRadiusAt(float now) { return SearchRadius + Math.Max(0, now - SearchObservedAt) * 1.5; }

    vector RouteDestination()
    {
        if (HasDetour) return Detour;
        return Goal;
    }
}

class PredateHunterBrain
{
    static float Distance(vector a, vector b)
    {
        vector delta = a - b;
        delta[1] = 0;
        return delta.Length();
    }

    static void Hold(PredateHunterState state, string mode, string reason)
    {
        if (state.HasGoal) state.GoalRevision++;
        state.HasGoal = false;
        state.HasDetour = false;
        state.WatchUntil = 0;
        state.Mode = mode;
        state.Reason = reason;
    }

    static void Goal(PredateHunterState state, vector ownPosition, vector destination, float now, string mode, string reason, string evidence)
    {
        state.Goal = destination;
        state.HasGoal = true;
        state.HasDetour = false;
        state.WatchUntil = 0;
        state.GoalRevision++;
        state.Mode = mode;
        state.Reason = reason;
        state.EvidenceKey = evidence;
        state.ProgressPosition = ownPosition;
        state.ProgressAt = now;
        state.RouteFailures = 0;
    }

    static float EvidenceScore(PredateObservation candidate, PredateObservationMemory memory, float now)
    {
        float score = candidate.ConfidenceAt(now) / (1 + candidate.RadiusAt(now) / 150);
        int counted = 1 << candidate.Source;
        if (candidate.Source == PredateObservationSource.AUDIBLE || candidate.Source == PredateObservationSource.IMPACT)
            counted |= (1 << PredateObservationSource.AUDIBLE) | (1 << PredateObservationSource.IMPACT);
        foreach (PredateObservation corroboration : memory.Entries)
        {
            if (!corroboration.HasPosition || corroboration.Expired(now) || Math.AbsFloat(corroboration.ObservedAt - candidate.ObservedAt) > 12) continue;
            if (candidate.SubjectId != "" && corroboration.SubjectId != "" && candidate.SubjectId != corroboration.SubjectId) continue;
            int source = 1 << corroboration.Source;
            if ((counted & source) != 0) continue;
            if (Distance(candidate.Estimate, corroboration.Estimate) > candidate.Radius + corroboration.Radius + 8) continue;
            score += corroboration.ConfidenceAt(now) * 0.12;
            counted |= source;
            // A shot and its impact are correlated.
            if (corroboration.Source == PredateObservationSource.AUDIBLE || corroboration.Source == PredateObservationSource.IMPACT)
                counted |= (1 << PredateObservationSource.AUDIBLE) | (1 << PredateObservationSource.IMPACT);
        }
        return score;
    }

    static PredateObservation LatestContact(PredateObservationMemory memory, float now, float retiredLifeAt = -1, string retiredSubject = "", string selectedSubject = "", string current = "")
    {
        PredateObservation best;
        float bestScore;
        foreach (PredateObservation observation : memory.Entries)
        {
            if (observation.Expired(now) || !observation.HasPosition) continue;
            if (observation.Source == PredateObservationSource.SIGNAL) continue;
            if ((observation.Source == PredateObservationSource.VISUAL || observation.Source == PredateObservationSource.GLIMPSE) && observation.ObservedAt <= retiredLifeAt && (retiredSubject == "" || observation.SubjectId == retiredSubject || observation.SubjectId == "")) continue;
            if (selectedSubject != "" && observation.SubjectId != "" && observation.SubjectId != selectedSubject) continue;
            float score = EvidenceScore(observation, memory, now);
            if (observation.EventKey == current) score += 0.08;
            if (best && (score < bestScore || (score == bestScore && observation.ObservedAt <= best.ObservedAt))) continue;
            best = observation;
            bestScore = score;
        }
        if (!best) return null;
        return best.Copy();
    }

    static bool SignalFix(PredateSignalTrack track, float now, out vector center, out float radius)
    {
        PredateObservation latest = track.LastSample;
        if (!latest || latest.Expired(now)) return false;
        for (int i = track.History.Count() - 2; i >= 0; i--)
        {
            PredateObservation previous = track.History[i];
            if (previous.Expired(now)) continue;
            PredateObservation previousRay = previous.Copy();
            PredateObservation latestRay = latest.Copy();
            if (previousRay.DistanceBand > 0) previousRay.Bearing = PredateMath.WrapHeading(previousRay.Bearing + previousRay.BearingError);
            if (latestRay.DistanceBand > 0) latestRay.Bearing = PredateMath.WrapHeading(latestRay.Bearing + latestRay.BearingError);
            if (!PredateInformationMath.IntersectBearings(previousRay, latestRay, 5000, center)) continue;
            if (latest.DistanceBand > 0 && Distance(center, PredateHuntSelection.SectorCenter(latest)) > PredateHuntSelection.SectorRadius(latest, now)) continue;
            if (previous.DistanceBand > 0 && Distance(center, PredateHuntSelection.SectorCenter(previous)) > PredateHuntSelection.SectorRadius(previous, now)) continue;
            float separation = Math.AbsFloat(Math.Sin((previousRay.Bearing - latestRay.Bearing) * Math.DEG2RAD));
            float distance = Distance(center, latest.Origin);
            float angular = distance * (previous.BearingError + latest.BearingError) * Math.DEG2RAD / Math.Max(0.0872, separation);
            radius = Math.Max(25, angular + (now - previous.ObservedAt) * 1.5);
            if (radius <= 500) return true;
        }
        return false;
    }

    static bool ConsistentArea(PredateHunterState state, PredateObservation signal, float now)
    {
        float distance = Distance(signal.Origin, state.SearchCenter);
        float heading = PredateMath.Bearing(signal.Origin, state.SearchCenter);
        float bearing = signal.Bearing;
        if (signal.DistanceBand > 0) bearing += PredateBearingPulse.DEGREES / 2.0;
        float difference = Math.AbsFloat(PredateMath.RelativeBearing(heading, bearing));
        float allowance = Math.Atan2(state.SearchRadiusAt(now), Math.Max(1, distance)) * Math.RAD2DEG;
        return difference <= allowance + signal.BearingError;
    }

    static vector SearchSector(PredateHunterState state, int sector, float now)
    {
        float spacing = Math.Clamp(state.SearchRadiusAt(now), 25, 120);
        return state.SearchCenter + PredateInformationMath.BearingDirection(sector * 45) * spacing;
    }

    static bool NextSearchPosition(PredateHunterState state, vector own, float now, out vector position)
    {
        float best = 1000000;
        for (int sector = 0; sector < 8; sector++)
        {
            if ((state.CheckedSectors & (1 << sector)) != 0) continue;
            vector point = SearchSector(state, sector, now);
            float distance = Distance(own, point);
            if (distance <= 12 || distance >= best) continue;
            best = distance;
            position = point;
        }
        return best < 1000000;
    }

    static void BeginFinalSearch(PredateHunterState state, vector own, PredateSignalTrack track, float now)
    {
        Hold(state, "observe", "Final Approach: stop, look and listen");
        state.EvidenceKey = "";
        state.SearchCenter = own;
        state.SearchRadius = 30;
        state.SearchObservedAt = now;
        state.SearchUntil = now + PredateBearingPulse.INTERVAL;
        state.CheckedSectors = 0;
        state.SearchStep = 0;
        state.WatchPoint = own + PredateInformationMath.BearingDirection(state.ScoutHeading) * 30;
        if (track && track.History.Count() > 0)
        {
            PredateObservation last = track.History[track.History.Count() - 1];
            if (!last.Expired(now))
            {
                vector toward = PredateHuntSelection.SectorCenter(last) - own;
                toward[1] = 0;
                if (toward.LengthSq() > 1)
                {
                    state.SearchCenter = own + toward.Normalized() * Math.Min(45, toward.Length());
                    state.WatchPoint = state.SearchCenter;
                }
                state.EvidenceKey = last.EventKey;
            }
        }
        state.WatchUntil = now + 3;
    }

    static void Step(PredateHunterState state, vector ownPosition, float now, PredateObservationMemory memory, PredateSignalTrack track, bool combat, bool recover, bool beingTracked = false, float retiredLifeAt = -1, string retiredSubject = "", string selectedSubject = "", bool replan = false, bool limited = false, bool finalApproach = false)
    {
        if (!state || !memory || memory.OwnerId != state.OwnerId || (track && track.OwnerId != state.OwnerId)) return;
        if (!beingTracked) state.InterceptionHandled = false;
        if (recover) { Hold(state, "recover", "Own medical or action priority"); return; }
        if (combat) { Hold(state, "combat", "Native tactical control after a lawful contact"); return; }
        bool freshApproach = finalApproach && (!state.FinalApproach || state.Mode == "combat" || state.Mode == "recover");
        bool leftApproach = state.FinalApproach && !finalApproach;
        if (now < state.NextDecisionAt && !freshApproach && !leftApproach) return;
        state.NextDecisionAt = now + 1;
        state.FinalApproach = finalApproach;
        bool workingSensoryClue = state.LastContactKey != "" && state.EvidenceKey == state.LastContactKey && state.SearchUntil > now && (state.HasGoal || state.WatchUntil > now);
        if (leftApproach)
        {
            state.LastSignalKey = "";
            if (!workingSensoryClue) Hold(state, "hold", "Outside Final Approach; return to scheduled bearings");
        }
        if (freshApproach && !workingSensoryClue) state.LastContactKey = "";
        if (!finalApproach && beingTracked && !state.InterceptionHandled)
        {
            state.InterceptionHandled = true;
            float heading = state.ScoutHeading;
            if (state.HasGoal && Distance(ownPosition, state.Goal) > 1) heading = PredateMath.Bearing(ownPosition, state.Goal);
            else if (track && track.LastSample) heading = track.LastSample.Bearing + track.LastSample.BearingError;
            vector escapeDirection = PredateInformationMath.BearingDirection(PredateMath.WrapHeading(heading + 90));
            state.SearchUntil = now + 45;
            Goal(state, ownPosition, ownPosition + escapeDirection * 90, now, "evade", "Reposition after an interception warning", "");
            return;
        }
        if (!finalApproach && state.Mode == "evade" && state.HasGoal && now < state.SearchUntil && Distance(ownPosition, state.Goal) > 8) return;
        PredateObservation contact = LatestContact(memory, now, retiredLifeAt, retiredSubject, selectedSubject, state.LastContactKey);
        // A new sound inside the same uncertainty area should not restart an active search.
        if (contact && contact.IsAudible() && state.HasGoal && now < state.SearchUntil && (state.Mode == "investigate" || state.Mode == "search") && Distance(state.SearchCenter, contact.Estimate) <= state.SearchRadiusAt(now) + contact.Radius)
        {
            state.LastContactKey = contact.EventKey;
            contact = null;
        }
        if (contact && contact.EventKey != state.LastContactKey)
        {
            state.LastContactKey = contact.EventKey;
            if (!state.HasGoal || Distance(state.SearchCenter, contact.Estimate) > Math.Max(15, state.SearchRadius)) state.CheckedSectors = 0;
            state.SearchCenter = contact.Estimate;
            state.SearchRadius = Math.Min(200, contact.RadiusAt(now));
            state.SearchObservedAt = now;
            state.SearchUntil = Math.Min(contact.ExpiresAt, now + 90);
            if (limited) state.SearchUntil = Math.Min(state.SearchUntil, now + 24);
            state.SearchStep = 0;
            bool watchFromHere = limited && (contact.Source != PredateObservationSource.VISUAL || Distance(ownPosition, contact.Estimate) > 140);
            bool quietStep = contact.Source == PredateObservationSource.FOOTSTEP && contact.Confidence < 0.4;
            if (contact.Source == PredateObservationSource.GLIMPSE || quietStep || watchFromHere)
            {
                string watchReason = "Check the direction of ambiguous movement";
                if (quietStep) watchReason = "Listen and look after a single footstep";
                if (watchFromHere) watchReason = "Watch the observed area from here while injured";
                Hold(state, "observe", watchReason);
                state.WatchPoint = contact.Estimate;
                state.WatchUntil = Math.Min(contact.ExpiresAt, now + 4);
                state.EvidenceKey = contact.EventKey;
                return;
            }
            string reason = "Investigate an observed area; attribution remains unchanged";
            if (contact.Source == PredateObservationSource.FOOTSTEP) reason = "Investigate an area after repeated nearby footsteps";
            if (contact.Source == PredateObservationSource.LIGHT) reason = "Investigate a visible weapon light";
            Goal(state, ownPosition, contact.Estimate, now, "investigate", reason, contact.EventKey);
            return;
        }
        if (freshApproach && !workingSensoryClue)
        {
            BeginFinalSearch(state, ownPosition, track, now);
            return;
        }
        if (now < state.WatchUntil) return;
        if (limited)
        {
            if (!state.HasGoal || (state.Mode != "investigate" && state.Mode != "search") || Distance(ownPosition, state.SearchCenter) > 140)
            {
                Hold(state, "hold", "Avoid extended pursuit while injured");
                return;
            }
            state.SearchUntil = Math.Min(state.SearchUntil, state.SearchObservedAt + 24);
        }
        if (finalApproach && !state.HasGoal)
        {
            if (state.Mode != "observe") return;
            vector localSearchPoint;
            if (now < state.SearchUntil && NextSearchPosition(state, ownPosition, now, localSearchPoint))
                Goal(state, ownPosition, localSearchPoint, now, "search", "Final Approach: check nearby ground for contact", state.EvidenceKey);
            else Hold(state, "hold", "Bounded search exhausted; no new knowledge");
            return;
        }
        PredateObservation signal;
        if (track && !limited && !finalApproach) signal = track.LastSample;
        if (signal && !signal.Expired(now) && signal.EventKey != state.LastSignalKey)
        {
            state.LastSignalKey = signal.EventKey;
            vector estimatedCenter;
            float estimatedRadius;
            if (SignalFix(track, now, estimatedCenter, estimatedRadius))
            {
                state.SearchCenter = estimatedCenter;
                state.SearchRadius = estimatedRadius;
                state.SearchObservedAt = now;
                state.SearchUntil = now + Math.Clamp(Distance(ownPosition, estimatedCenter) / 2 + 60, 120, 240);
                state.SearchStep = 0;
                state.CheckedSectors = 0;
                vector approach = vector.Direction(ownPosition, estimatedCenter);
                approach[1] = 0;
                approach.Normalize();
                vector destination = estimatedCenter - approach * Math.Min(100, estimatedRadius * 0.3);
                Goal(state, ownPosition, destination, now, "approach", "Approach overlapping historical bearing areas", signal.EventKey);
            }
            else
            {
                // A weak/parallel new baseline does not erase an existing area
                // that is still compatible with this independent observation.
                if (state.HasGoal && (state.Mode == "approach" || state.Mode == "search") && now < state.SearchUntil && ConsistentArea(state, signal, now))
                {
                    state.EvidenceKey = signal.EventKey;
                    state.Reason = "New ray supports continued approach to the uncertain area";
                    return;
                }
                state.SearchUntil = signal.ExpiresAt;
                vector forward = PredateInformationMath.BearingDirection(signal.Bearing);
                if (signal.DistanceBand > 0)
                {
                    vector estimate = PredateHuntSelection.SectorCenter(signal);
                    vector towardsSector = estimate - ownPosition;
                    towardsSector[1] = 0;
                    if (towardsSector.LengthSq() > 1) forward = towardsSector.Normalized();
                    float travel = Math.Clamp((towardsSector.Length() - 100) * 0.55, 35, 300);
                    vector flank = Vector(forward[2], 0, -forward[0]);
                    if (replan) flank = -flank;
                    vector flankGoal = ownPosition + forward * travel + flank * Math.Clamp(travel * 0.45, 35, 85);
                    Goal(state, ownPosition, flankGoal, now, "observe", "Close distance from a different bearing", signal.EventKey);
                    return;
                }
                vector lateral = Vector(forward[2], 0, -forward[0]);
                vector observationPosition = ownPosition + lateral * 80 + forward * 60;
                Goal(state, ownPosition, observationPosition, now, "observe", "Create a lateral baseline for the next scheduled sample", signal.EventKey);
            }
            return;
        }
        if (!state.HasGoal)
        {
            if (state.Mode == "combat" || state.Mode == "recover") state.LastContactKey = "";
            if (state.Mode != "reassess") Hold(state, "hold", "Await independent evidence or the next scheduled sample");
            return;
        }
        if (now >= state.SearchUntil)
        {
            string exhaustedReason = "Evidence search lifetime exhausted";
            if (finalApproach) exhaustedReason = "Bounded search exhausted; no new knowledge";
            Hold(state, "hold", exhaustedReason);
            return;
        }
        if (Distance(ownPosition, state.Goal) <= 8)
        {
            if (state.Mode == "scout")
            {
                state.ScoutAt = now + 12;
                Hold(state, "hold", "Observe from the scouting waypoint");
                return;
            }
            if (state.Mode == "observe" || state.Mode == "evade")
            {
                Hold(state, "hold", "Observation baseline reached; wait for scheduled sample");
                return;
            }
            int searchLimit = 4 + (state.OwnerId.Hash() & 3);
            if (limited) searchLimit = 2;
            if (now >= state.SearchUntil || state.SearchStep >= searchLimit)
            {
                Hold(state, "hold", "Bounded search exhausted; no new knowledge");
                return;
            }
            if (state.SearchWatchRevision != state.GoalRevision)
            {
                state.SearchWatchRevision = state.GoalRevision;
                state.WatchPoint = state.SearchCenter;
                state.WatchUntil = now + 2.0;
                return;
            }
            state.SearchStep++;
            vector uncheckedPosition;
            if (NextSearchPosition(state, ownPosition, now, uncheckedPosition))
                Goal(state, ownPosition, uncheckedPosition, now, "search", "Check an unobserved part of the remembered area", state.EvidenceKey);
            else Hold(state, "hold", "Bounded search exhausted; no new knowledge");
            return;
        }
        if (state.HasDetour && Distance(ownPosition, state.Detour) <= 8)
        {
            state.HasDetour = false;
            state.GoalRevision++;
            state.ProgressPosition = ownPosition;
            state.ProgressAt = now;
            state.Reason = "Resume the original destination after detour";
            return;
        }
        if (Distance(ownPosition, state.ProgressPosition) >= 4)
        {
            state.ProgressPosition = ownPosition;
            state.ProgressAt = now;
        }
        else if (now - state.ProgressAt >= 20)
        {
            state.RouteFailures++;
            state.ProgressAt = now;
            if (state.RouteFailures >= 3)
            {
                Hold(state, "reassess", "Route blocked repeatedly; wait for new evidence");
                return;
            }
            vector detourDirection = vector.Direction(ownPosition, state.Goal);
            detourDirection[1] = 0;
            detourDirection.Normalize();
            vector detourSide = Vector(detourDirection[2], 0, -detourDirection[0]);
            if (state.RouteFailures == 2) detourSide = -detourSide;
            state.Detour = ownPosition + detourSide * 25 + detourDirection * 15;
            state.HasDetour = true;
            state.GoalRevision++;
            state.Reason = "Local route alternative after measured lack of progress";
        }
    }
}
