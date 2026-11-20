modded class AnalyticsManagerClient
{
    override void Event_OnEntityKilled(EntityAI victim, EntityAI killer, EntityAI source, bool is_headshot)
    {
        // A distant Operator may have died outside the client's replication range.
        if (!victim || !GetGame().GetPlayer()) return;
        super.Event_OnEntityKilled(victim, killer, source, is_headshot);
    }
}
