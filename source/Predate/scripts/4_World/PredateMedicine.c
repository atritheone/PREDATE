modded class MorphineMdfr
{
    float PredateExpectedBonusHealing(PlayerBase player)
    {
        // At full blood one dose adds 34.2 HP on top of 1.8 HP from ordinary regeneration.
        return player.GetHealthRegenSpeed() * 19.0 * LIFETIME;
    }

    override void OnTick(PlayerBase player, float deltaT)
    {
        if (!player.IsAlive()) return;

        // The engine advances the modifier's age before the tick. Trim its last tick to the dose duration.
        float activeSeconds = Math.Clamp(LIFETIME - (GetAttachedTime() - deltaT), 0, deltaT);
        float extraHealth = PredateExpectedBonusHealing(player) * (activeSeconds / LIFETIME);
        player.AddHealth("GlobalHealth", "Health", extraHealth);
    }
}
