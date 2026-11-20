modded class ExpansionStatic
{
    override static string DumpToString(InventoryLocation loc)
    {
        // Expansion's trace formats the destination even when tracing is off.
        // DayZ's formatter does not parse instance IDs on newly cloned attachments.
        if (!loc) return "{ null }";
        return loc.DumpToString();
    }
}
