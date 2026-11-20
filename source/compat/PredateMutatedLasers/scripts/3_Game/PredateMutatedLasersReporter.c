modded class MutatedWatchlistReporter_MutatedLaser
{
    // Suppress usage reporting at both the public entry point and the HTTP sender.
    override void SendUsagePing_ML()
    {
    }

    override void MWL_ML_SendWebhook(string webhook, string serverName, string modName, string tag)
    {
    }
}
