void main() {}

class PredateQuadNodShadowReport
{
    ref array<string> Models = {};
    ref array<string> Lods = {};
    ref array<float> Up = {};
    ref array<float> Down = {};
    ref array<float> StowedAgain = {};
    ref array<bool> Mounted = {};
    int Captures;
}

class PredateQuadNodShadowMission : MissionGameplay
{
    ref PredateQuadNodShadowReport m_Report = new PredateQuadNodShadowReport;
    ref array<NVGoggles> m_Goggles = {};
    Camera m_ProbeCamera;
    PlayerBase m_Human;
    eAIBase m_Bot;
    ref eAIGroup m_Group;
    vector m_Site;

    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SetUp, 4500);
    }

    vector AtGround(vector point)
    {
        point[1] = GetGame().SurfaceY(point[0], point[2]);
        return point;
    }

    NVGoggles Equip(PlayerBase actor, string type)
    {
        EntityAI strap = actor.GetInventory().CreateAttachment("NVGHeadstrap");
        if (!strap) return null;
        NVGoggles item = NVGoggles.Cast(strap.GetInventory().CreateAttachment(type));
        m_Report.Mounted.Insert(item != null);
        return item;
    }

    void SetUp()
    {
        GetGame().GetWorld().SetDate(2026, 6, 21, 16, 0);
        Weather weather = GetGame().GetWeather();
        weather.GetOvercast().Set(0, 0, 0);
        weather.GetRain().Set(0, 0, 0);
        weather.GetFog().Set(0, 0, 0);

        m_Site = AtGround("4497 0 10312");
        m_Human = PlayerBase.Cast(GetGame().CreatePlayer(null, "SurvivorM_Mirek", AtGround(m_Site + "-0.9 0 0"), 0, "NONE"));
        if (m_Human)
        {
            GetGame().SelectPlayer(null, m_Human);
            m_Human.SetAllowDamage(false);
            m_Human.SetDirection("0 0 1");
            m_Goggles.Insert(Equip(m_Human, "FOG_GPNVG_Tan"));
        }
        m_Bot = eAIBase.Cast(GetGame().CreateObjectEx("eAI_SurvivorM_Mirek", AtGround(m_Site + "0.9 0 0"), ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
        if (m_Bot)
        {
            m_Group = eAIGroup.CreateGroup(new eAIFactionObservers);
            m_Bot.SetGroup(m_Group);
            m_Bot.SetMovementSpeedLimits(0, 0);
            m_Bot.m_PredateProbeCeaseFire = true;
            m_Bot.SetAllowDamage(false);
            m_Bot.SetDirection("0 0 1");
            m_Goggles.Insert(Equip(m_Bot, "FOG_GPNVG_Black"));
        }
        NVGoggles gsgm = NVGoggles.Cast(GetGame().CreateObjectEx("FOG_GPNVG_GSGM_Tan", m_Site + "-0.33 0.73 -0.6", ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
        NVGoggles elite = NVGoggles.Cast(GetGame().CreateObjectEx("FOG_GPNVG_Elite", m_Site + "0.33 0.73 -0.6", ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
        m_Goggles.Insert(gsgm);
        m_Goggles.Insert(elite);
        m_ProbeCamera = Camera.Cast(GetGame().CreateObject("staticcamera", m_Site + "-0.35 5.3 3.4", true));
        if (m_ProbeCamera)
        {
            m_ProbeCamera.LookAt(m_Site + "0.5 0.75 0");
            m_ProbeCamera.SetFOV(0.48);
            m_ProbeCamera.SetActive(true);
        }
        array<LOD> lods = {};
        foreach (NVGoggles item : m_Goggles)
        {
            if (!item) continue;
            string model;
            GetGame().ConfigGetText("CfgVehicles " + item.GetType() + " model", model);
            m_Report.Models.Insert(model);
            lods.Clear();
            item.GetLODS(lods);
            string names;
            names = "";
            foreach (LOD lod : lods)
                names += " " + item.GetLODName(lod);
            m_Report.Lods.Insert(names);
            item.SetAnimationPhase("rotate", 0);
        }
        Save();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CaptureUp, 3500);
    }

    void Save()
    {
        string error;
        JsonFileLoader<PredateQuadNodShadowReport>.SaveFile("$profile:QuadNodShadowReport.json", m_Report, error);
    }

    void CaptureUp()
    {
        MakeScreenshot("$profile:QuadNodsUp.bmp");
        m_Report.Captures++;
        foreach (NVGoggles item : m_Goggles)
        {
            if (!item) continue;
            m_Report.Up.Insert(item.GetAnimationPhase("rotate"));
            item.SetAnimationPhase("rotate", 1);
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CaptureDown, 3500);
    }

    void CaptureDown()
    {
        MakeScreenshot("$profile:QuadNodsDown.bmp");
        m_Report.Captures++;
        foreach (NVGoggles item : m_Goggles)
        {
            if (!item) continue;
            m_Report.Down.Insert(item.GetAnimationPhase("rotate"));
            item.SetAnimationPhase("rotate", 0);
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Finish, 3500);
    }

    void Finish()
    {
        foreach (NVGoggles item : m_Goggles)
            if (item) m_Report.StowedAgain.Insert(item.GetAnimationPhase("rotate"));
        Save();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(GetGame().RequestExit, 2000, false, 0);
    }
}

Mission CreateCustomMission(string path)
{
    return new PredateQuadNodShadowMission;
}
