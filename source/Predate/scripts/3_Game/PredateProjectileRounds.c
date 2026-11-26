class PredateProjectileContact
{
    Object Target;
    vector Position;

    void PredateProjectileContact(Object target, vector position)
    {
        Target = target;
        Position = position;
    }
}

class PredateProjectileRound
{
    string Tag;
    string BaseAmmo;
    Object Weapon;
    int Muzzle;
    float CartridgeDamage;
    float Lifetime;
    float ExpiresAt;
    vector Origin;
    Object ContactTarget;
    vector ContactPosition;
    Object FirstOperator;
    ref set<Object> AdditionalOperators;
    int Deployment;
    int Number;
    bool Fired;
    bool Hit;
    bool Closed;
    ref array<ref PredateProjectileContact> EarlyContacts;
}

class PredateProjectileRounds
{
    static const int SLOT_COUNT = 192;
    static const string PREFIX = "PR_";
    static bool Enabled;
    static int NextSlot;
    static ref map<string, ref PredateProjectileRound> Active = new map<string, ref PredateProjectileRound>;
    static ref map<string, float> Lifetimes = new map<string, float>;
    static ref ScriptInvoker OperatorContact = new ScriptInvoker;
    static ref ScriptInvoker RejectedLaunch = new ScriptInvoker;

    static PredateProjectileRound Find(string ammo)
    {
        PredateProjectileRound round;
        if (Active.Find(ammo, round) && round.Fired && !round.Closed) return round;
        return null;
    }

    static void ReportContact(string ammo, Object target, vector position, bool damageFallback = false)
    {
        if (!Enabled || !target || Active.Count() == 0) return;
        PredateProjectileRound round;
        if (!Active.Find(ammo, round) || round.Closed) return;
        if (damageFallback && round.ContactTarget == target) return;
        if (round.Number > 0)
        {
            OperatorContact.Invoke(round, target, position, false);
            return;
        }
        // Native impact can arrive during TryFireWeapon, before OnFire
        // confirms the discharge and assigns the round to its deployment.
        Man person = Man.Cast(target);
        EntityAI entity = EntityAI.Cast(target);
        if (!person && entity) person = entity.GetHierarchyRootPlayer();
        if (!person || !person.IsAlive()) return;
        if (!round.EarlyContacts) round.EarlyContacts = new array<ref PredateProjectileContact>;
        round.EarlyContacts.Insert(new PredateProjectileContact(target, position));
    }

    static string OriginalAmmo(string ammo)
    {
        if (ammo.IndexOf(PREFIX) != 0) return ammo;
        string suffix = ammo.Substring(PREFIX.Length(), ammo.Length() - PREFIX.Length());
        int separator = suffix.IndexOf("_");
        if (separator < 0 || separator + 1 >= suffix.Length()) return ammo;
        return suffix.Substring(separator + 1, suffix.Length() - separator - 1);
    }
}
