// Native rigid bodies continue simulating when only mission updates are held.
// Hold the world's actual bodies, retaining collision and their resume momentum.
class PredateWorldPhysicsEntry
{
    IEntity BodyEntity;
    Physics BodyPhysics;
    vector LinearVelocity;
    vector AngularVelocity;
    bool WasActive;
}

class PredateWorldPhysicsHold
{
    ref array<ref PredateWorldPhysicsEntry> m_Entries = new array<ref PredateWorldPhysicsEntry>;

    bool Contains(IEntity entity, Physics body)
    {
        foreach (PredateWorldPhysicsEntry entry : m_Entries)
            if (entry.BodyEntity == entity && entry.BodyPhysics == body) return true;
        return false;
    }

    void Capture(IEntity anchor)
    {
        if (!anchor) return;
        array<ref PredateWorldPhysicsEntry> additions = new array<ref PredateWorldPhysicsEntry>;
        int count = PhysicsWorld.GetNumDynamicBodies(anchor);
        // Changing state changes the native enumeration. Capture first, mutate
        // second. Characters use their separate animation/clock hold.
        for (int i = 0; i < count; i++)
        {
            IEntity entity = PhysicsWorld.GetDynamicBody(anchor, i);
            if (!entity || Man.Cast(entity)) continue;
            Physics body = entity.GetPhysics();
            if (!body || !body.IsDynamic() || body.GetSimulationState() != SimulationState.SIMULATION || Contains(entity, body)) continue;
            PredateWorldPhysicsEntry entry = new PredateWorldPhysicsEntry;
            vector linear = GetVelocity(entity);
            vector angular = body.GetAngularVelocity();
            bool active = body.IsActive();
            entry.BodyEntity = entity;
            entry.BodyPhysics = body;
            entry.LinearVelocity = linear;
            entry.AngularVelocity = angular;
            entry.WasActive = active;
            additions.Insert(entry);
        }
        foreach (PredateWorldPhysicsEntry held : additions)
        {
            if (!held.BodyEntity) continue;
            Physics physics = held.BodyEntity.GetPhysics();
            if (!physics || physics != held.BodyPhysics) continue;
            physics.ChangeSimulationState(SimulationState.COLLISION);
            m_Entries.Insert(held);
        }
    }

    void Resume()
    {
        foreach (PredateWorldPhysicsEntry entry : m_Entries)
        {
            if (!entry.BodyEntity) continue;
            Physics body = entry.BodyEntity.GetPhysics();
            // A replaced/destroyed body must never inherit old momentum.
            if (!body || body != entry.BodyPhysics) continue;
            body.ChangeSimulationState(SimulationState.SIMULATION);
            SetVelocity(entry.BodyEntity, entry.LinearVelocity);
            dBodySetAngularVelocity(entry.BodyEntity, entry.AngularVelocity);
            if (entry.WasActive) body.SetActive(ActiveState.ACTIVE);
            else body.SetActive(ActiveState.INACTIVE);
        }
        m_Entries.Clear();
    }
}
