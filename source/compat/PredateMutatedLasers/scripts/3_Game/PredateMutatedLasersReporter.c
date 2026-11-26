modded class MutatedWatchlistReporter_MutatedLaser
{
    // Suppress usage reporting at both the public entry point and the HTTP sender.
    override void SendUsagePing_ML()
    {
    }

    override void MWL_ML_SendWebhook(string mwl_ml_webhook, string mwl_ml_serverName, string mwl_ml_modName, string mwl_ml_tag)
    {
    }
}
