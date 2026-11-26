modded class MM_LaserSight_Base
{
    protected bool m_PredateLaserIR;
    protected bool m_PredateIRLight;
#ifndef SERVER
    protected ScriptedLightBase m_PredateIRSpot;
    protected bool m_PredateRenderedLaserIR;
    protected bool m_PredateTrackedPEQVisuals;
    protected static ref array<MM_LaserSight_Base> s_PredatePEQVisuals = {};
#endif

    void MM_LaserSight_Base()
    {
        RegisterNetSyncVariableBool("m_PredateLaserIR");
        RegisterNetSyncVariableBool("m_PredateIRLight");
    }

    static MM_LaserSight_Base PredateOnWeapon(ItemBase item)
    {
        if (!Weapon_Base.Cast(item)) return null;
        return MM_LaserSight_Base.Cast(item.FindAttachmentBySlotName("weaponFlashlight"));
    }

    override bool PredateSupportsWeaponControl(int control)
    {
        return control >= PredateWeaponControl.WHITE_LIGHT && control <= PredateWeaponControl.INFRARED_LIGHT;
    }

#ifdef SERVER
    override protected int PredatePackWeaponModuleState()
    {
        int state = super.PredatePackWeaponModuleState();
        if (m_MM_FlashModeSynch) state |= 2;
        if (m_MM_LaserModeSynch) state |= 4;
        if (m_PredateLaserIR) state |= 8;
        if (m_PredateIRLight) state |= 16;
        if (m_MM_FlashStrobeSynch) state |= 32;
        return state;
    }

    override protected void PredateApplyWeaponModuleState(int state, bool drawn)
    {
        m_MM_FlashModeSynch = (state & 2) != 0;
        m_MM_LaserModeSynch = (state & 4) != 0;
        m_PredateLaserIR = (state & 8) != 0;
        m_PredateIRLight = (state & 16) != 0;
        m_MM_FlashStrobeSynch = (state & 32) != 0;
        m_MM_VisibleBeamsEnabledSynch = !m_PredateLaserIR;
        super.PredateApplyWeaponModuleState(state, drawn);
    }
#endif

    override void PredateUseWeaponControl(PlayerBase player, int control)
    {
        ComponentEnergyManager energy = GetCompEM();
        bool enablingWhiteLight = control == PredateWeaponControl.WHITE_LIGHT && !m_MM_FlashModeSynch;
        bool enablingLaser = control == PredateWeaponControl.LASER_POWER && !m_MM_LaserModeSynch;
        bool enablingIlluminator = control == PredateWeaponControl.INFRARED_LIGHT && !m_PredateIRLight;
        if ((enablingWhiteLight || enablingLaser || enablingIlluminator) && (!energy || !energy.CanWork()))
        {
            player.PredateWeaponModuleMessage("Weapon module needs a working battery.");
            return;
        }

        string status;
        bool active;
        switch (control)
        {
            case PredateWeaponControl.WHITE_LIGHT:
                m_MM_FlashModeSynch = !m_MM_FlashModeSynch;
                m_MM_FlashStrobeSynch = false;
                status = "White Light";
                active = m_MM_FlashModeSynch;
                break;
            case PredateWeaponControl.LASER_POWER:
                m_MM_LaserModeSynch = !m_MM_LaserModeSynch;
                status = PredateLaserModeName();
                active = m_MM_LaserModeSynch;
                break;
            case PredateWeaponControl.LASER_MODE:
                m_PredateLaserIR = !m_PredateLaserIR;
                break;
            case PredateWeaponControl.INFRARED_LIGHT:
                m_PredateIRLight = !m_PredateIRLight;
                status = "IR Illuminator";
                active = m_PredateIRLight;
                break;
        }
        m_MM_VisibleBeamsEnabledSynch = !m_PredateLaserIR;
        MM_EnsureEnergyState_Server();
        if (control == PredateWeaponControl.LASER_MODE)
        {
            if (m_PredateLaserIR) player.PredateWeaponModuleMessage("Laser Mode IR");
            else player.PredateWeaponModuleMessage("Laser Mode Visible");
            return;
        }
        if (active) player.PredateWeaponModuleMessage(status + " On");
        else player.PredateWeaponModuleMessage(status + " Off");
    }

    protected string PredateLaserModeName()
    {
        if (m_PredateLaserIR) return "IR Laser";
        return "Visible Laser";
    }

    void PredateUseOperatorModule(bool ready, bool gogglesActive, bool dark)
    {
        bool powered = ready && (!dark || gogglesActive) && GetCompEM().CanWork();
        bool infraredLight = powered && dark;
        if (m_MM_LaserModeSynch == powered && m_PredateLaserIR == dark && !m_MM_FlashModeSynch && m_PredateIRLight == infraredLight)
            return;

        m_PredateLaserIR = dark;
        m_MM_VisibleBeamsEnabledSynch = !dark;
        m_MM_LaserModeSynch = powered;
        m_MM_FlashModeSynch = false;
        m_MM_FlashStrobeSynch = false;
        m_PredateIRLight = infraredLight;
        MM_EnsureEnergyState_Server();
    }

    override protected void MM_ApplyServerConfig_Server()
    {
        super.MM_ApplyServerConfig_Server();
        if (GetGame().IsServer()) m_MM_VisibleBeamsEnabledSynch = !m_PredateLaserIR;
    }

    override protected bool MM_ShouldOutputBeActive_Server()
    {
        return m_PredateIRLight || super.MM_ShouldOutputBeActive_Server();
    }

    override bool CheckNVGState()
    {
#ifdef SERVER
        return false;
#else
        PPERequester_CameraNV nightVision = PPERequester_CameraNV.Cast(PPERequesterBank.GetRequester(PPERequesterBank.REQ_CAMERANV));
        return nightVision && nightVision.PredateCanSeeInfrared();
#endif
    }

#ifndef SERVER
    void ~MM_LaserSight_Base()
    {
        // The Workshop destructor calls g_Game.ObjectDelete on this beam.
        // Engine shutdown already owns the object if the game is gone.
        if (!GetGame()) m_Mutated_LaserBeam = null;
    }

    override protected vector MM_GetViewerLaserColor()
    {
        if (m_PredateLaserIR) return "0.82 0.94 1";
        return super.MM_GetViewerLaserColor();
    }

    override protected string MM_GetDotClassname()
    {
        if (m_PredateLaserIR) return "Mutated_Laser_Dot_IceBlue";
        return super.MM_GetDotClassname();
    }

    override void OnVariablesSynchronized()
    {
        if (m_PredateRenderedLaserIR != m_PredateLaserIR)
        {
            MM_DeleteDotModels();
            MM_DeleteBeam();
            m_PredateRenderedLaserIR = m_PredateLaserIR;
        }
        super.OnVariablesSynchronized();
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(PredateUpdateIRLight);
        PredateUpdateIRLight();
        if (m_PredateIRLight) GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(PredateUpdateIRLight, 100, true);
    }

    override protected void MM_EnsureBeamObject()
    {
        super.MM_EnsureBeamObject();
        if (!m_Mutated_LaserBeam || m_PredateTrackedPEQVisuals) return;
        m_PredateTrackedPEQVisuals = true;
        s_PredatePEQVisuals.Insert(this);
    }

    static void PredateReleasePEQVisuals()
    {
        if (!GetGame()) return;
        foreach (MM_LaserSight_Base module : s_PredatePEQVisuals)
        {
            if (!module) continue;
            module.MM_CleanupLaserVisuals_Client(true);
            module.MM_DeleteDotModels();
            module.m_PredateTrackedPEQVisuals = false;
        }
        s_PredatePEQVisuals.Clear();
    }

    override protected void MM_DeleteDotModel(out EntityAI dotObj)
    {
        if (GetGame()) super.MM_DeleteDotModel(dotObj);
        else dotObj = null;
    }

    protected void PredateUpdateIRLight()
    {
        Man holder = GetHierarchyRootPlayer();
        Weapon_Base weapon = Weapon_Base.Cast(GetHierarchyParent());
        bool show = m_PredateIRLight && GetCompEM().IsWorking() && holder && weapon && holder.GetHumanInventory().GetEntityInHands() == weapon && CheckNVGState();
        if (show && !m_PredateIRSpot)
        {
            m_PredateIRSpot = ScriptedLightBase.CreateLight(PredateModuleSpot);
            if (m_PredateIRSpot) m_PredateIRSpot.AttachOnMemoryPoint(this, "beamStart", "beamEnd");
        }
        else if (!show && m_PredateIRSpot)
        {
            GetGame().ObjectDelete(m_PredateIRSpot);
            m_PredateIRSpot = null;
        }
    }

    override void EEDelete(EntityAI parent)
    {
        if (m_PredateTrackedPEQVisuals)
        {
            s_PredatePEQVisuals.RemoveItem(this);
            m_PredateTrackedPEQVisuals = false;
        }
        if (GetGame())
        {
            MM_DeleteBeam();
            MM_DeleteDotModels();
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(PredateUpdateIRLight);
            if (m_PredateIRSpot) GetGame().ObjectDelete(m_PredateIRSpot);
            m_PredateIRSpot = null;
        }
        super.EEDelete(parent);
    }
#endif
}
