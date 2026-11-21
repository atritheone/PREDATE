class PredateServerDiagnostics
{
    static bool Enabled;
    static bool TraceOperators;
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
