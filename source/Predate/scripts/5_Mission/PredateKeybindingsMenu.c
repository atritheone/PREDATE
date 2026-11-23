#ifndef SERVER
modded class KeybindingsMenu
{
    override Widget Init()
    {
        if (!IsCLIParam("predateSessionId") || !MissionGameplay.Cast(GetGame().GetMission())) return super.Init();

        // DayZ caches this map. Filter it while the tabs and their matching panels are built.
        InputUtils.InitInputMetadata();
        map<int, ref array<int>> sorting = InputUtils.GetInputActionSortingMap();
        map<int, ref array<int>> original = new map<int, ref array<int>>;
        foreach (int originalGroup, array<int> originalActions : sorting)
            original.Insert(originalGroup, originalActions);
        array<int> unsorted = InputUtils.GetUnsortedInputActions();
        array<int> originalUnsorted = {};
        originalUnsorted.Copy(unsorted);
        array<int> multiplayer = {};
        PredateSoloInterface.MultiplayerInputs(multiplayer);
        UAInput fire = GetUApi().GetInputByName("UAFire");
        int weapons = -1;
        int standard = -1;
        int predate = -1;
        int expansion = -1;
        for (int index = 0; index < sorting.Count(); index++)
        {
            int key = sorting.GetKey(index);
            string label = GetUApi().SortingLocalization(key);
            if (label == "PREDATE") predate = key;
            else if (label == "STR_EXPANSION_LABEL") expansion = key;
            else
            {
                array<int> bindings = sorting.GetElement(index);
                if (!bindings || bindings.Count() == 0) continue;
                if (standard < 0) standard = key;
                if (fire && bindings.Find(fire.ID()) >= 0) weapons = key;
            }
        }
        if (weapons < 0) weapons = standard;

        array<int> predateBindings = sorting.Get(predate);
        array<int> expansionBindings = sorting.Get(expansion);
        array<int> standardBindings = sorting.Get(weapons);
        array<int> hidden;
        if (predateBindings && standardBindings)
        {
            array<int> combined = new array<int>;
            combined.Copy(standardBindings);
            foreach (int action : predateBindings)
                if (combined.Find(action) < 0) combined.Insert(action);
            sorting.Set(weapons, combined);
        }
        if (predateBindings) sorting.Set(predate, hidden);
        if (expansionBindings) sorting.Set(expansion, hidden);
        foreach (int group, array<int> actions : sorting)
        {
            if (!actions) continue;
            array<int> filtered = new array<int>;
            filtered.Copy(actions);
            foreach (int multiplayerAction : multiplayer)
                filtered.RemoveItem(multiplayerAction);
            if (filtered.Count() != actions.Count()) sorting.Set(group, filtered);
        }
        foreach (int hiddenAction : multiplayer) unsorted.RemoveItem(hiddenAction);
        Widget root = super.Init();
        foreach (int restoredGroup, array<int> restoredActions : original)
            sorting.Set(restoredGroup, restoredActions);
        unsorted.Copy(originalUnsorted);
        return root;
    }
}
#endif
