class PredateRockRecovery
{
    static const float MAX_DISTANCE = 6;

    static vector BodyPoint(PlayerBase actor, string bone, vector fallback)
    {
        int index = actor.GetBoneIndexByName(bone);
        if (index < 0) return fallback;
        vector point = actor.GetBonePositionWS(index);
        if (!PredateInformationMath.FiniteVector(point)) return fallback;
        float offset = vector.DistanceSq(point, actor.GetPosition());
        if (offset < 0.0001 || vector.DistanceSq(point, fallback) > 4) return fallback;
        return point;
    }

    static Object EmbeddedRock(eAIBase actor, vector body)
    {
        array<Object> rocks = {};
        if (!GetGame().IsBoxCollidingGeometry(body, vector.Zero, "0.28 0.38 0.28", ObjIntersectGeom, ObjIntersectGeom, {actor}, rocks, CollisionFlags.ONLYSTATIC)) return null;
        foreach (Object rock : rocks)
            if (rock && rock.IsRock()) return rock;
        return null;
    }

    static bool Occluded(PlayerBase player, eAIBase actor, vector eye, vector point)
    {
        if (vector.DistanceSq(eye, point) < 0.04) return false;
        vector hit, normal;
        int component;
        return DayZPhysics.RaycastRV(eye, point, hit, normal, component, null, actor, player, false, false, ObjIntersectView, 0);
    }

    static bool FindExit(eAIBase actor, PlayerBase player, vector position, vector body, vector eye, Object rock, out vector exit)
    {
        array<Object> excluded = {actor};
        set<Object> hits = new set<Object>;
        vector playerPosition = player.GetPosition();
        float closest = MAX_DISTANCE * MAX_DISTANCE + 0.01;
        bool found;
        for (float radius = 1; radius <= MAX_DISTANCE; radius += 1)
        {
            if (found && (radius - 0.6) * (radius - 0.6) >= closest) break;
            for (int direction = 0; direction < 8; direction++)
            {
                vector probe = position + PredateInformationMath.BearingDirection(direction * 45) * radius;
                vector site;
                if (!PredateRoutePlanner.Snap(probe, 0.6, site) || !PredateRoutePlanner.Land(site)) continue;
                float distance = vector.DistanceSq(position, site);
                if (distance < 0.25 || distance >= closest || Math.AbsFloat(site[1] - position[1]) > 1.5) continue;
                if (vector.DistanceSq(playerPosition, site) < 4) continue;
                if (GetGame().IsBoxCollidingGeometry(site + "0 0.95 0", vector.Zero, "0.65 1.7 0.65", ObjIntersectGeom, ObjIntersectGeom, excluded)) continue;
                // Test from the exit inward, so the first obstacle can be the rock containing the actor.
                vector contact, normal;
                int component;
                hits.Clear();
                if (DayZPhysics.RaycastRV(site + "0 0.85 0", body, contact, normal, component, hits, null, actor, true, false, ObjIntersectGeom, 0, CollisionFlags.ONLYSTATIC))
                {
                    if (hits.Count() == 0 || hits[0] != rock) continue;
                }
                if (!Occluded(player, actor, eye, site + "0 0.85 0") || !Occluded(player, actor, eye, site + "0 1.65 0")) continue;
                closest = distance;
                exit = site;
                found = true;
            }
        }
        return found;
    }
}

modded class eAIBase
{
    protected string m_PredateStalledRouteMode;
    protected int m_PredateStalledRouteRevision;
    protected vector m_PredateStalledRoutePosition;
    protected float m_PredateNextRockCheck;

    void PredateQueueRockRecovery(PredateHunterState state, string routeMode, vector position, float now)
    {
        m_PredateStalledRouteMode = routeMode;
        m_PredateStalledRouteRevision = state.GoalRevision;
        m_PredateStalledRoutePosition = position;
        // Verify the actor stays in place, even if its fall animation never ends.
        m_PredateNextRockCheck = now + 1;
    }

    // A true result means the recovery used one of the shared path-planning slots.
    bool PredateTryRockRecovery(PlayerBase player, PredateHunterState state, vector position, float now)
    {
        if (m_PredateStalledRouteMode == "") return false;
        if (vector.DistanceSq(position, m_PredateStalledRoutePosition) > 1)
        {
            m_PredateStalledRouteMode = "";
            return false;
        }
        if (now < m_PredateNextRockCheck || IsSwimming() || IsClimbing() || IsFighting() || GetWeaponManager().IsRunning()) return false;
        m_PredateNextRockCheck = now + 8;

        vector body = PredateRockRecovery.BodyPoint(this, "Pelvis", position + "0 0.85 0");
        Object rock = PredateRockRecovery.EmbeddedRock(this, body);
        if (!rock)
        {
            m_PredateStalledRouteMode = "";
            return true;
        }
        vector eye = PredateRockRecovery.BodyPoint(player, "Head", player.GetPosition() + "0 1.6 0");
        vector head = PredateRockRecovery.BodyPoint(this, "Head", position + "0 1.65 0");
        if (!PredateRockRecovery.Occluded(player, this, eye, body) || !PredateRockRecovery.Occluded(player, this, eye, head)) return true;
        vector exit;
        if (!PredateRockRecovery.FindExit(this, player, position, body, eye, rock, exit)) return true;

        SetPosition(exit);
        eAI_ResetPathfinding();
        if (state.Mode == "reassess" && state.GoalRevision == m_PredateStalledRouteRevision && now < state.SearchUntil)
        {
            state.Mode = m_PredateStalledRouteMode;
            state.HasGoal = true;
            state.Reason = "Resume the route after escaping a rock";
        }
        if (state.HasGoal) state.GoalRevision++;
        state.HasDetour = false;
        state.RouteFailures = 0;
        state.ProgressAt = now;
        state.ProgressPosition = exit;
        m_PredateStalledRouteMode = "";
        if (PredateServerDiagnostics.TraceOperators)
            Print("[PredateRoute] rock recovery; operator=" + state.OwnerId + "; from=" + position + "; to=" + exit);
        return true;
    }
}
