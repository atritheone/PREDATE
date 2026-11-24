class PredateActionPrompts
{
    protected ref map<string, TextWidget> m_Labels = new map<string, TextWidget>;

    void Init(Widget root, string prefix)
    {
        root.SetColor(0);
        Widget item = root.FindAnyWidget(prefix + "item");
        if (item) item.SetColor(0);

        array<string> rows = {"interact", "continuous_interact", "single", "continuous"};
        foreach (string name : rows)
        {
            string rowName = prefix + name;
            Widget row = root.FindAnyWidget(rowName);
            if (!row) continue;
            TextWidget label = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets("Predate/gui/action_prompt.layout", row));
            if (!label) continue;
            Widget nativeLabel = row.FindAnyWidget(rowName + "_action_name");
            if (nativeLabel) nativeLabel.Show(false);
            HideIcons(row, rowName);
            m_Labels.Insert(rowName, label);
        }
    }

    protected static void HideIcons(Widget row, string rowName)
    {
        Widget pcIcon = row.FindAnyWidget(rowName + "_btn_icon");
        Widget controllerIcon = row.FindAnyWidget(rowName + "_btn_icon_xbox");
        if (pcIcon && pcIcon.IsVisible()) pcIcon.Show(false);
        if (controllerIcon && controllerIcon.IsVisible()) controllerIcon.Show(false);
    }

    static string KeyName(bool interact)
    {
        string inputName = "UADefaultAction";
        if (interact) inputName = "UAAction";
        UAInput input = GetUApi().GetInputByName(inputName);
        for (int alternative = 0; alternative < input.AlternativeCount(); alternative++)
        {
            input.SelectAlternative(alternative);
            if (!input.BindingCount() || !input.CheckBindDevice(0, EUAINPUT_DEVICE_KEYBOARDMOUSE)) continue;
            string key;
            for (int index = 0; index < input.BindingCount(); index++)
            {
                int button = input.Binding(index);
                if (!button) continue;
                string name = GetUApi().GetButtonName(button);
                if (button == "mBLeft".Hash() || name == "Left Mouse Button") name = "LMB";
                if (key != "") key += " + ";
                key += name;
            }
            return key;
        }
        return "";
    }

    static string FormatLabel(string key, string description, bool hold)
    {
        if (key == "" || description == "") return description;
        if (hold) key = "Hold " + key;
        return key + "  " + description;
    }

    void Update(ActionBase action, string description, string rowName)
    {
        TextWidget label = m_Labels.Get(rowName);
        if (!label || !action || !label.GetParent().IsVisible()) return;
        HideIcons(label.GetParent(), rowName);
        ActionInput input = action.GetInput();
        bool hold = input && input.GetInputType() == ActionInputType.AIT_CONTINUOUS;
        if (action.IsInherited(ActionInjectSelf)) description = "Inject yourself";
        else if (action.IsInherited(ActionBandageSelf)) description = "Bandage yourself";
        label.SetText(FormatLabel(KeyName(rowName.Contains("interact")), description, hold));
    }
}

modded class ActionTargetsCursor
{
    protected ref PredateActionPrompts m_PredatePrompts;

    override protected void OnWidgetScriptInit(Widget w)
    {
        super.OnWidgetScriptInit(w);
        m_PredatePrompts = new PredateActionPrompts;
        m_PredatePrompts.Init(w, "");
    }

    override protected void SetActionWidget(ActionBase action, string descText, string actionWidget, string descWidget)
    {
        super.SetActionWidget(action, descText, actionWidget, descWidget);
        if (m_PredatePrompts) m_PredatePrompts.Update(action, descText, actionWidget);
    }
}

modded class ItemActionsWidget
{
    protected ref PredateActionPrompts m_PredatePrompts;

    override protected void OnWidgetScriptInit(Widget w)
    {
        super.OnWidgetScriptInit(w);
        m_PredatePrompts = new PredateActionPrompts;
        m_PredatePrompts.Init(w, "ia_");
    }

    override protected void SetActionWidget(ActionBase action, string descText, string actionWidget, string descWidget)
    {
        super.SetActionWidget(action, descText, actionWidget, descWidget);
        if (m_PredatePrompts) m_PredatePrompts.Update(action, descText, actionWidget);
    }
}
