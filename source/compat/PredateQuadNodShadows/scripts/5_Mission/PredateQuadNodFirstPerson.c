#ifndef SERVER
modded class MissionGameplay
{
    protected PlayerBase m_PredateQuadNodWearer;
    protected ItemBase m_PredateQuadNodCaster;
    protected int m_PredateQuadNodHead;
    protected float m_PredateQuadNodPhase;

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        PlayerBase wearer = PlayerBase.Cast(GetGame().GetPlayer());
        if (wearer != m_PredateQuadNodWearer)
        {
            PredateRemoveQuadNodCaster();
            m_PredateQuadNodWearer = wearer;
            if (wearer) m_PredateQuadNodHead = wearer.GetBoneIndexByName("Head");
        }
        if (!wearer || !wearer.IsAlive() || wearer.IsInThirdPerson() || m_PredateQuadNodHead < 0)
        {
            PredateRemoveQuadNodCaster();
            return;
        }

        EntityAI strap = wearer.FindAttachmentBySlotName("Eyewear");
        if (!strap || !strap.IsKindOf("NVGHeadstrap"))
        {
            PredateRemoveQuadNodCaster();
            return;
        }
        NVGoggles goggles = NVGoggles.Cast(strap.FindAttachmentBySlotName("NVG"));
        if (!goggles)
        {
            PredateRemoveQuadNodCaster();
            return;
        }
        string casterType;
        if (goggles.IsKindOf("FOG_GPNVG_Elite")) casterType = "PredateQuadNodEliteCaster";
        else if (goggles.IsKindOf("FOG_GPNVG_GSGM_Base")) casterType = "PredateQuadNodGSGMCaster";
        else if (goggles.IsKindOf("FOG_GPNVG_Base")) casterType = "PredateQuadNodCaster";
        if (casterType == string.Empty)
        {
            PredateRemoveQuadNodCaster();
            return;
        }
        if (m_PredateQuadNodCaster && m_PredateQuadNodCaster.GetType() != casterType)
            PredateRemoveQuadNodCaster();
        if (!m_PredateQuadNodCaster)
        {
            m_PredateQuadNodCaster = ItemBase.Cast(GetGame().CreateObjectEx(casterType, wearer.GetPosition(),
                ECE_LOCAL | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN));
            if (!m_PredateQuadNodCaster) return;
            m_PredateQuadNodCaster.SetTakeable(false);
            m_PredateQuadNodPhase = -1;
        }

        // The hidden NVG proxy reports a different transform in first person.
        // Position the caster at the front of the animated head instead.
        vector head[4], mount[4], transform[4];
        wearer.GetBoneTransformWS(m_PredateQuadNodHead, head);
        mount[0] = "0 0 1";
        mount[1] = "-1 0 0";
        mount[2] = "0 -1 0";
        mount[3] = "0.124 0.154 0";
        Math3D.MatrixMultiply4(head, mount, transform);
        m_PredateQuadNodCaster.SetTransform(transform);

        float phase = goggles.GetAnimationPhase("rotate");
        if (phase != m_PredateQuadNodPhase)
        {
            m_PredateQuadNodCaster.SetAnimationPhase("rotate", phase);
            m_PredateQuadNodPhase = phase;
        }
    }

    protected void PredateRemoveQuadNodCaster()
    {
        if (!m_PredateQuadNodCaster) return;
        GetGame().ObjectDelete(m_PredateQuadNodCaster);
        m_PredateQuadNodCaster = null;
    }

    override void OnMissionFinish()
    {
        PredateRemoveQuadNodCaster();
        m_PredateQuadNodWearer = null;
        super.OnMissionFinish();
    }
}
#endif
