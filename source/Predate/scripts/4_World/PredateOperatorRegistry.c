// Runtime references never enter JSON. Only a registered storage key can bind an actor.
class PredateOperatorSlot
{
    ref PredateOperatorRecord Record;
}

class PredateOperatorIndex
{
    ref map<string, eAIBase> Actors = new map<string, eAIBase>;
    ref set<string> Duplicates = new set<string>;

    static PredateOperatorIndex Capture()
    {
        PredateOperatorIndex index = new PredateOperatorIndex;
        foreach (eAIBase actor : eAIBase.eAI_GetAll())
        {
            if (!actor || !actor.IsAlive()) continue;
            eAIGroup group = actor.GetGroup();
            if (!group || group.m_BaseName == "") continue;
            string key = group.m_BaseName;
            eAIBase previous;
            if (!index.Actors.Find(key, previous)) index.Actors.Insert(key, actor);
            else if (previous != actor && index.Duplicates.Find(key) == -1) index.Duplicates.Insert(key);
        }
        return index;
    }
}

class PredateOperatorRegistry
{
    ref map<string, ref PredateOperatorSlot> m_Slots = new map<string, ref PredateOperatorSlot>;

    bool Register(PredateOperatorRecord record)
    {
        if (!record) return false;
        if (m_Slots.Contains(record.Id)) return false;
        foreach (string id, PredateOperatorSlot existing : m_Slots)
            if (existing.Record.StorageKey == record.StorageKey) return false;
        PredateOperatorSlot slot = new PredateOperatorSlot;
        slot.Record = record;
        m_Slots.Insert(record.Id, slot);
        return true;
    }

    PredateOperatorSlot Get(string id) { return m_Slots.Get(id); }

    bool Bind(string id, eAIGroup group, eAIBase actor)
    {
        PredateOperatorSlot slot = Get(id);
        if (!slot) return false;
        if (!group) return false;
        if (!actor) return false;
        if (actor.GetGroup() != group) return false;
        if (group.m_BaseName != slot.Record.StorageKey) return false;
        return true;
    }

    // Refuse ambiguity instead of adopting whichever AI happened to be enumerated first.
    bool Resolve(string id, out eAIBase resolvedActor, out eAIGroup resolvedGroup, out string error, PredateOperatorIndex index = null)
    {
        // A defeated slot resolves successfully without a body. Clear the
        // caller's outputs even when it reuses the previous slot's variables.
        resolvedActor = null;
        resolvedGroup = null;
        PredateOperatorSlot slot = Get(id);
        if (!slot) { error = "Unregistered operator " + id; return false; }
        if (slot.Record.LifeState == "defeated") return true;
        eAIGroup foundGroup;
        eAIBase foundActor;
        if (index)
        {
            if (index.Duplicates.Find(slot.Record.StorageKey) != -1)
            {
                error = "Duplicate living actors for " + id;
                return false;
            }
            if (index.Actors.Find(slot.Record.StorageKey, foundActor)) foundGroup = foundActor.GetGroup();
        }
        // Expansion replaces AI entities when a client joins. Its active-entity
        // registry updates during replacement before old group-member references
        // are necessarily safe to dereference. Resolve identity from live entities.
        else foreach (eAIBase candidate : eAIBase.eAI_GetAll())
        {
            if (!candidate) continue;
            eAIGroup group = candidate.GetGroup();
            if (!group) continue;
            if (group.m_BaseName != slot.Record.StorageKey) continue;
            if (!candidate.IsAlive()) continue;
            if (foundActor && foundActor != candidate)
            {
                error = "Duplicate living actors for " + id;
                return false;
            }
            foundGroup = group;
            foundActor = candidate;
        }
        if (foundActor)
        {
            resolvedActor = foundActor;
            resolvedGroup = foundGroup;
            return true;
        }
        if (PredateOperatorDeaths.Contains(slot.Record.StorageKey))
        {
            slot.Record.LifeState = "defeated";
            return true;
        }
        error = "Missing living actor for " + id;
        return false;
    }
}

class PredateOperatorDeaths
{
    static ref array<string> Keys = new array<string>;
    static ref ScriptInvoker Defeated = new ScriptInvoker;
    static bool Contains(string key) { return Keys.Find(key) >= 0; }

    static PlayerBase Attacker(Object cause)
    {
        PlayerBase attacker = PlayerBase.Cast(cause);
        EntityAI source = EntityAI.Cast(cause);
        if (!attacker && source) attacker = PlayerBase.Cast(source.GetHierarchyRootPlayer());
        Grenade_Base grenade = Grenade_Base.Cast(cause);
        if (!attacker && grenade) attacker = grenade.m_PredateExplosionOwner;
        return attacker;
    }

    static PlayerBase HuntPlayer()
    {
        array<Man> players = {};
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (player && player.GetIdentity()) return player;
        }
        return null;
    }
}

modded class eAIBase
{
    PredateOperatorRecord m_PredateOperatorRecord;
    bool m_PredateBleedingDeath;
    bool m_PredatePlayerBleedout;
    protected bool m_PredateDeferDeathFiles;

    override void EEKilled(Object killer)
    {
        eAIGroup group = GetGroup();
        if (group)
        {
            if (group.m_BaseName != "" && !PredateOperatorDeaths.Contains(group.m_BaseName))
            {
                PredateOperatorDeaths.Keys.Insert(group.m_BaseName);
                int slot = PredateProtocol.OperatorSlot(m_PredateOperatorId);
                if (GetGame().IsServer() && slot >= 0)
                {
                    PlayerBase credited = PredateOperatorDeaths.Attacker(killer);
                    bool playerInjury = m_PredateOperatorRecord && m_PredateOperatorRecord.PlayerHealthBeforeInjury > 0;
                    if ((!credited || credited == this) && (m_PredatePlayerBleedout || (!m_PredateBleedingDeath && playerInjury)))
                        credited = PredateOperatorDeaths.HuntPlayer();
                    if (credited && credited.GetIdentity() && credited.m_PredateKillNotices.Find(slot) < 0)
                        credited.m_PredateKillNotices.Insert(slot);
                    PredateOperatorDeaths.Defeated.Invoke(slot, credited);
                }
            }
        }
        // Expansion queues removal from the group after EEKilled. For an
        // operator's one-member group, that cleanup deletes all its files.
        m_PredateDeferDeathFiles = m_PredateOperatorRecord && group && group.m_BaseName == m_PredateOperatorRecord.StorageKey && group.Count() == 1 && group.GetMember(0) == this;
        super.EEKilled(killer);
        m_PredateDeferDeathFiles = false;
    }

    override void eAI_DeletePersistentFiles()
    {
        if (!m_PredateDeferDeathFiles) super.eAI_DeletePersistentFiles();
    }
}
