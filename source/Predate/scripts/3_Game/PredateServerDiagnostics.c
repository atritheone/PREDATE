class PredatePerfHistogram
{
    ref array<int> m_Buckets = {};
    int m_Count;
    float m_Sum;
    float m_Maximum;

    void Add(float milliseconds)
    {
        int bucket = Math.Clamp(Math.Ceil(milliseconds), 0, 1000);
        while (m_Buckets.Count() <= bucket) m_Buckets.Insert(0);
        m_Buckets[bucket] = m_Buckets[bucket] + 1;
        m_Count++;
        m_Sum += milliseconds;
        m_Maximum = Math.Max(m_Maximum, milliseconds);
    }

    int Percentile(float fraction)
    {
        if (m_Count == 0) return 0;
        int threshold = Math.Ceil(m_Count * fraction);
        int cumulative;
        foreach (int bucket, int count : m_Buckets)
        {
            cumulative += count;
            if (cumulative >= threshold) return bucket;
        }
        return 0;
    }

    string Report()
    {
        return "samples=" + m_Count + " total_ms=" + m_Sum + " p50_ms=" + Percentile(0.5) + " p95_ms=" + Percentile(0.95) + " p99_ms=" + Percentile(0.99) + " max_ms=" + m_Maximum;
    }

    void Reset()
    {
        m_Buckets.Clear();
        m_Count = 0;
        m_Sum = 0;
        m_Maximum = 0;
    }
}

class PredateServerDiagnostics
{
    static bool Enabled;
    static bool Performance;
    static bool HasPerfPlayer;
    static vector PerfPlayerPosition;
    static ref PredatePerfHistogram NearAI;
    static ref PredatePerfHistogram EngagedAI;
    static ref PredatePerfHistogram RemoteAI;
    static bool TraceOperators;
    static bool TraceShots;
    static ref set<Object> ShotSources;

    static void StartAIProfile()
    {
        NearAI = new PredatePerfHistogram;
        EngagedAI = new PredatePerfHistogram;
        RemoteAI = new PredatePerfHistogram;
    }

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
