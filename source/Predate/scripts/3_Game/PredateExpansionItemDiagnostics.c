modded class ExpansionStatic
{
    #ifdef DIAG
    override static string GetDebugInfo(Class instance, bool checkParent = true)
    {
        if (!instance) return "null";
        // Some restored attachments have no bracketed ID in the diagnostic runtime.
        string description = instance.ToString();
        int start = description.LastIndexOf("<");
        if (start < 0 || description.LastIndexOf(">") <= start + 1) return instance.GetDebugName();
        return super.GetDebugInfo(instance, checkParent);
    }
    #endif

    override static string DumpToString(InventoryLocation loc)
    {
        // Expansion's trace formats the destination even when tracing is off.
        // DayZ's formatter does not parse instance IDs on newly cloned attachments.
        if (!loc) return "{ null }";
        return loc.DumpToString();
    }
}
