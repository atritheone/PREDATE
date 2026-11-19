class PredateMemoryPersistence
{
    static void Capture(PredateSession session)
    {
        PredateWorldState state = session.m_WorldState;
        if (!state) return;
        if ((session.m_ActorsActive && !session.m_Paused) || state.Year == 0)
        {
            int year, month, day, hour, minute;
            GetGame().GetWorld().GetDate(year, month, day, hour, minute);
            state.Year = year;
            state.Month = month;
            state.Day = day;
            state.Hour = hour;
            state.Minute = minute;
        }
        if (session.m_HuntGroup)
        {
            state.WaypointIndex = session.m_HuntGroup.m_CurrentWaypointIndex;
            state.CurrentWaypoint = session.m_HuntGroup.m_CurrentWaypoint;
            state.Backtracking = session.m_HuntGroup.m_BackTracking;
        }
        eAIBase bot = session.m_HuntBot;
        if (!bot) return;
        if (!bot.IsAlive()) return;
        state.BotHealth = bot.GetHealth("", "Health");
        state.BotBlood = bot.GetHealth("", "Blood");
        state.BotShock = bot.GetHealth("", "Shock");
        state.HasBotVitals = true;
        // Native target timers use engine uptime, which keeps advancing in menus.
        // Retain the last active observation throughout loading/reconnect grace.
        if (!session.m_ActorsActive || session.m_Paused) return;
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase connected = PlayerBase.Cast(man);
            if (!connected || !connected.GetIdentity()) continue;
            foreach (PredateContactMemory previous : state.Contacts)
                if (previous.PlayerId == connected.GetIdentity().GetId()) previous.RemainingMilliseconds = 0;
        }
        // Only currently tracked targets repopulate the remaining lifetime.
        // Otherwise a forgotten contact could reappear after a save/reopen.
        foreach (eAITarget target : bot.GetTargets())
        {
            if (!target) continue;
            PlayerBase player = PlayerBase.Cast(target.GetEntity());
            if (!player) continue;
            if (!player.GetIdentity() || !player.IsAlive() || player.m_PredateStagedForDeployment) continue;
            PredateContactMemory saved;
            foreach (PredateContactMemory existing : state.Contacts)
                if (existing.PlayerId == player.GetIdentity().GetId()) saved = existing;
            if (!saved)
            {
                saved = new PredateContactMemory;
                saved.PlayerId = player.GetIdentity().GetId();
                state.Contacts.Insert(saved);
            }
            saved.LastKnownPosition = target.m_LastKnownPosition;
            saved.SearchPosition = target.m_SearchPosition;
            saved.Threat = target.m_ThreatLevel;
            saved.ActiveThreat = target.m_ThreatLevelActive;
            int remaining = target.GetRemainingTime();
            saved.RemainingMilliseconds = Math.Max(0, remaining);
            saved.SearchUpdates = target.m_SearchPositionUpdateCount;
            saved.SearchOnLOSLost = target.m_SearchOnLOSLost;
        }
    }

    static void RestoreContact(PredateSession session, PlayerBase player)
    {
        if (session.m_MemoryRestored) return;
        if (!session.m_HuntBot) return;
        if (!session.m_WorldState) return;
        foreach (PredateContactMemory saved : session.m_WorldState.Contacts)
        {
            if (saved.PlayerId != player.GetIdentity().GetId()) continue;
            if (saved.RemainingMilliseconds <= 0) continue;
            eAITargetInformation info = player.GetTargetInformation();
            eAITarget target = session.m_HuntBot.eAI_GetTargetInformationState(info, false);
            if (!target) continue;
            target.SetInitial(saved.Threat, saved.LastKnownPosition);
            target.m_ThreatLevelActive = saved.ActiveThreat;
            target.m_SearchPosition = saved.SearchPosition;
            target.m_SearchPositionUpdateCount = saved.SearchUpdates;
            target.m_SearchOnLOSLost = saved.SearchOnLOSLost;
            target.m_LOS = false; // A saved observation is not current visual contact.
            if (!target.m_IsTracked) target.Track(saved.RemainingMilliseconds);
            else
            {
                target.m_FoundAtTime = GetGame().GetTime();
                target.m_MaxTime = saved.RemainingMilliseconds;
            }
            Print("[Predate] Restored private contact memory without sampling the player's current position.");
        }
        session.m_MemoryRestored = true;
    }
}
