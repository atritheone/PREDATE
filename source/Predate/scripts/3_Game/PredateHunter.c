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
    int GoalRevision;
    vector SearchCenter;
    float SearchRadius;
    float SearchObservedAt;
    float SearchUntil;
    int SearchStep;
    float NextDecisionAt;
    vector ProgressPosition;
    float ProgressAt;
    int RouteFailures;
    bool InterceptionHandled;
    float ScoutAt;
    float ScoutHeading;

    bool Validate(float now, string expectedOwner = "raven-01")
    {
        if (Schema != 1 || OwnerId != expectedOwner || GoalRevision < 0 || SearchStep < 0 || RouteFailures < 0 || RouteFailures > 3) return false;
        if (!PredateInformationMath.FiniteVector(Goal) || !PredateInformationMath.FiniteVector(SearchCenter) || !PredateInformationMath.FiniteVector(ProgressPosition)) return false;
        if (!PredateInformationMath.Finite(NextDecisionAt) || NextDecisionAt < 0 || !PredateInformationMath.Finite(ProgressAt) || ProgressAt < 0 || ProgressAt > now) return false;
        if (!PredateInformationMath.Finite(SearchObservedAt) || SearchObservedAt < 0 || SearchObservedAt > now) return false;
        if (!PredateInformationMath.Finite(SearchRadius) || SearchRadius < 0 || SearchRadius > 1000 || !PredateInformationMath.Finite(SearchUntil) || SearchUntil < 0) return false;
        if (!PredateInformationMath.Finite(ScoutAt) || ScoutAt < 0 || !PredateInformationMath.Finite(ScoutHeading) || ScoutHeading < 0 || ScoutHeading >= 360) return false;
        return Mode == "hold" || Mode == "observe" || Mode == "evade" || Mode == "approach" || Mode == "search" || Mode == "investigate" || Mode == "combat" || Mode == "recover" || Mode == "reassess" || Mode == "scout";
    }

    float SearchRadiusAt(float now) { return SearchRadius + Math.Max(0, now - SearchObservedAt) * 1.5; }
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
        state.Mode = mode;
        state.Reason = reason;
    }

    static void Goal(PredateHunterState state, vector ownPosition, vector destination, float now, string mode, string reason, string evidence)
    {
        state.Goal = destination;
        state.HasGoal = true;
        state.GoalRevision++;
        state.Mode = mode;
        state.Reason = reason;
        state.EvidenceKey = evidence;
        state.ProgressPosition = ownPosition;
        state.ProgressAt = now;
        state.RouteFailures = 0;
    }

    static PredateObservation LatestContact(PredateObservationMemory memory, float now, float retiredLifeAt = -1, string retiredSubject = "", string selectedSubject = "")
    {
        PredateObservation best;
        foreach (PredateObservation observation : memory.Entries)
        {
            if (observation.Expired(now) || !observation.HasPosition) continue;
            if (observation.Source == PredateObservationSource.SIGNAL) continue;
            if (observation.Source == PredateObservationSource.VISUAL && observation.ObservedAt <= retiredLifeAt && (retiredSubject == "" || observation.SubjectId == retiredSubject)) continue;
            if (selectedSubject != "" && observation.SubjectId != "" && observation.SubjectId != selectedSubject) continue;
            if (!best || observation.ObservedAt > best.ObservedAt) best = observation;
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

    static void Step(PredateHunterState state, vector ownPosition, float now, PredateObservationMemory memory, PredateSignalTrack track, bool combat, bool recover, bool beingTracked = false, float retiredLifeAt = -1, string retiredSubject = "", string selectedSubject = "", bool replan = false)
    {
        if (!state || !memory || memory.OwnerId != state.OwnerId || (track && track.OwnerId != state.OwnerId)) return;
        if (!beingTracked) state.InterceptionHandled = false;
        if (recover) { Hold(state, "recover", "Own medical or action priority"); return; }
        if (combat) { Hold(state, "combat", "Native tactical control after a lawful contact"); return; }
        if (now < state.NextDecisionAt) return;
        state.NextDecisionAt = now + 1;
        if (beingTracked && !state.InterceptionHandled)
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
        if (state.Mode == "evade" && state.HasGoal && now < state.SearchUntil && Distance(ownPosition, state.Goal) > 8) return;
        PredateObservation contact = LatestContact(memory, now, retiredLifeAt, retiredSubject, selectedSubject);
        if (contact && contact.EventKey != state.LastContactKey)
        {
            state.LastContactKey = contact.EventKey;
            state.SearchCenter = contact.Estimate;
            state.SearchRadius = Math.Min(200, contact.RadiusAt(now));
            state.SearchObservedAt = now;
            state.SearchUntil = Math.Min(contact.ExpiresAt, now + 90);
            state.SearchStep = 0;
            string reason = "Investigate an observed area; attribution remains unchanged";
            if (contact.Source == PredateObservationSource.LIGHT) reason = "Investigate a visible weapon light";
            Goal(state, ownPosition, contact.Estimate, now, "investigate", reason, contact.EventKey);
            return;
        }
        PredateObservation signal;
        if (track) signal = track.LastSample;
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
            Hold(state, "hold", "Evidence search lifetime exhausted");
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
            if (now >= state.SearchUntil || state.SearchStep >= 4)
            {
                Hold(state, "hold", "Bounded search exhausted; no new knowledge");
                return;
            }
            state.SearchStep++;
            float angle = state.SearchStep * 137.5;
            vector offset = PredateInformationMath.BearingDirection(angle) * Math.Clamp(state.SearchRadiusAt(now), 25, 120);
            vector searchDestination = state.SearchCenter + offset;
            Goal(state, ownPosition, searchDestination, now, "search", "Search an uncertain remembered area", state.EvidenceKey);
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
            state.Goal = ownPosition + detourSide * 25 + detourDirection * 15;
            state.GoalRevision++;
            state.Reason = "Local route alternative after measured lack of progress";
        }
    }
}
