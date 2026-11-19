modded class ItemActionsWidget
{
    override protected void BuildCursor()
    {
        super.BuildCursor();
        if (!m_Root) return;
        // Keep contextual actions; hide the held-item name, condition and ammo row.
        Widget readout = m_Root.FindAnyWidget("ia_item");
        if (readout) readout.Show(false);
    }
}
