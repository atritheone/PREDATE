class PredateServerDiagnostics
{
    static bool Enabled;
    static bool TraceOperators;
    static bool TraceShots;
    static ref set<Object> ShotSources;

    static void RememberShotSource(Object source)
    {
        if (!ShotSources) ShotSources = new set<Object>;
        ShotSources.Insert(source);
    }

    static bool TraceShotSource(Object source)
    {
        if (!source) return false;
        if (ShotSources && ShotSources.Find(source) >= 0) return true;
        EntityAI entity = EntityAI.Cast(source);
        if (!entity) return false;
        Man shooter = entity.GetHierarchyRootPlayer();
        if (!shooter) shooter = Man.Cast(entity);
        return shooter && shooter.GetIdentity();
    }
}

modded class DayZGame
{
    override void FirearmEffects(Object source, Object directHit, int componentIndex, string surface, vector pos, vector surfNormal, vector exitPos, vector inSpeed, vector outSpeed, bool isWater, bool deflected, string ammoType)
    {
        if (PredateServerDiagnostics.TraceShots && PredateServerDiagnostics.TraceShotSource(source))
            Print("[PredateShot] effect " + GetTime() + " " + ammoType + " source=" + source + " hit=" + directHit + " position=" + pos + " exitSpeed=" + outSpeed + " deflected=" + deflected);
        PredateProjectileRounds.ReportContact(ammoType, directHit, pos);
        super.FirearmEffects(source, directHit, componentIndex, surface, pos, surfNormal, exitPos, inSpeed, outSpeed, isWater, deflected, PredateProjectileRounds.OriginalAmmo(ammoType));
    }

    override void OnProjectileStopped(ProjectileStoppedInfo info)
    {
        if (PredateServerDiagnostics.TraceShots && PredateServerDiagnostics.TraceShotSource(info.GetSource()))
            Print("[PredateShot] stop " + GetTime() + " " + info.GetAmmoType() + " source=" + info.GetSource() + " position=" + info.GetPos());
        super.OnProjectileStopped(info);
    }

    override void OnProjectileStoppedInObject(ObjectCollisionInfo info)
    {
        if (PredateServerDiagnostics.TraceShots && PredateServerDiagnostics.TraceShotSource(info.GetSource()))
            Print("[PredateShot] object " + GetTime() + " " + info.GetAmmoType() + " source=" + info.GetSource() + " hit=" + info.GetHitObj() + " position=" + info.GetPos());
        if (PredateProjectileRounds.Enabled && PredateProjectileRounds.Active.Count() > 0)
            PredateProjectileRounds.ReportContact(info.GetAmmoType(), info.GetHitObj(), info.GetPos());
        super.OnProjectileStoppedInObject(info);
    }

    override void OnProjectileStoppedInTerrain(TerrainCollisionInfo info)
    {
        if (PredateServerDiagnostics.TraceShots && PredateServerDiagnostics.TraceShotSource(info.GetSource()))
            Print("[PredateShot] terrain " + GetTime() + " " + info.GetAmmoType() + " source=" + info.GetSource() + " position=" + info.GetPos());
        super.OnProjectileStoppedInTerrain(info);
    }
}

modded class EXHitch
{
    override void Log()
    {
        // Storage can emit a message for each inventory in a single checkpoint.
        // Diagnostics already reports the full checkpoint cost in one line.
        if (g_Game && g_Game.IsDedicatedServer() && m_MsgPrefix == "[ExpansionEntityStorage] ")
        {
            if (!PredateServerDiagnostics.Enabled) return;
            if (!PredateServerDiagnostics.TraceOperators && GetElapsedMS() < 50) return;
        }
        super.Log();
    }
}
