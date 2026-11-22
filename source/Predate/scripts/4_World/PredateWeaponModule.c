enum PredateWeaponModuleMode
{
    WHITE_LIGHT,
    VISIBLE_LASER,
    INFRARED_LASER,
    INFRARED_LIGHT
}

class PredateWeaponModule extends UniversalLight
{
    protected int m_PredateMode;
#ifndef SERVER
    protected ScriptedLightBase m_PredateEmitter;
    protected Shape m_PredateInfraredBeam;
    protected int m_PredateRenderedMode = -1;
    protected bool m_PredateLaserHit;
    protected vector m_PredateLaserPoint;
#endif

    void PredateWeaponModule()
    {
        RegisterNetSyncVariableInt("m_PredateMode", PredateWeaponModuleMode.WHITE_LIGHT, PredateWeaponModuleMode.INFRARED_LIGHT);
    }

    static PredateWeaponModule OnWeapon(ItemBase weapon)
    {
        if (!Weapon_Base.Cast(weapon)) return null;
        return PredateWeaponModule.Cast(weapon.FindAttachmentBySlotName("weaponFlashlight"));
    }

    void CycleMode(PlayerBase player)
    {
        m_PredateMode = (m_PredateMode + 1) % (PredateWeaponModuleMode.INFRARED_LIGHT + 1);
        SetSynchDirty();
        ReportMode(player);
    }

    void TogglePower(PlayerBase player)
    {
        ComponentEnergyManager power = GetCompEM();
        if (power.IsSwitchedOn()) power.SwitchOff();
        else if (power.CanWork()) power.SwitchOn();
        else
        {
            player.PredateWeaponModuleMessage("Weapon module needs a working battery.");
            return;
        }
        ReportMode(player);
    }

    protected void ReportMode(PlayerBase player)
    {
        string state = " Off";
        if (GetCompEM().IsSwitchedOn()) state = " On";
        player.PredateWeaponModuleMessage(ModeName() + state);
    }

    string ModeName()
    {
        switch (m_PredateMode)
        {
            case PredateWeaponModuleMode.VISIBLE_LASER: return "Visible Laser";
            case PredateWeaponModuleMode.INFRARED_LASER: return "IR Laser";
            case PredateWeaponModuleMode.INFRARED_LIGHT: return "IR Illuminator";
        }
        return "White Light";
    }

    override bool IsLightSource()
    {
        return m_PredateMode <= PredateWeaponModuleMode.VISIBLE_LASER;
    }

    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_PredateMode);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version)) return false;
        if (!ctx.Read(m_PredateMode)) return false;
        m_PredateMode = Math.Clamp(m_PredateMode, PredateWeaponModuleMode.WHITE_LIGHT, PredateWeaponModuleMode.INFRARED_LIGHT);
        return true;
    }

    override void OnWorkStart()
    {
#ifndef SERVER
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(UpdateEmitter);
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(UpdateEmitter, 33, true);
#endif
    }

    override void OnWork(float consumed_energy)
    {
        // The render update follows the holder's animation between energy ticks.
    }

    override void OnWorkStop()
    {
#ifndef SERVER
        StopEmitter();
#endif
    }

    override void EEDelete(EntityAI parent)
    {
#ifndef SERVER
        if (GetGame()) StopEmitter();
#endif
        super.EEDelete(parent);
    }

#ifndef SERVER
    bool PredateEmitterVisible()
    {
        return m_PredateEmitter != null;
    }

    bool PredateInfraredBeamVisible()
    {
        return m_PredateInfraredBeam != null;
    }

    bool LaserRay(out vector start, out vector direction)
    {
        if (!MemoryPointExists("beamStart") || !MemoryPointExists("beamEnd")) return false;
        start = ModelToWorld(GetMemoryPointPos("beamStart"));
        direction = ModelToWorld(GetSelectionPositionLS("beamEnd")) - start;
        if (direction.LengthSq() < 0.000001) return false;
        direction.Normalize();
        return true;
    }

    bool LaserVisualColor(out int color)
    {
        if (m_PredateMode != PredateWeaponModuleMode.VISIBLE_LASER || !GetCompEM().IsWorking() || !ViewerCanSeeEmitter()) return false;
        color = 0xFFFF514A;
        return true;
    }

    bool LaserScreenMark(out vector center)
    {
        if (!m_PredateLaserHit || m_PredateRenderedMode != m_PredateMode) return false;
        center = GetGame().GetScreenPosRelative(m_PredateLaserPoint);
        return center[2] > 0 && center[0] > 0 && center[0] < 1 && center[1] > 0 && center[1] < 1;
    }

    protected void StopEmitter()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(UpdateEmitter);
        DeleteEmitter();
    }

    protected void DeleteEmitter()
    {
        ClearLight();
        ClearInfraredBeam();
        m_PredateRenderedMode = -1;
        m_PredateLaserHit = false;
    }

    protected void ClearLight()
    {
        if (m_PredateEmitter) GetGame().ObjectDelete(m_PredateEmitter);
        m_PredateEmitter = null;
    }

    protected void ClearInfraredBeam()
    {
        if (m_PredateInfraredBeam) m_PredateInfraredBeam.Destroy();
        m_PredateInfraredBeam = null;
    }

    protected bool ViewerCanSeeEmitter()
    {
        PlayerBase viewer = PlayerBase.Cast(GetGame().GetPlayer());
        if (!viewer || !viewer.IsAlive() || viewer.IsUnconscious()) return false;
        if (vector.DistanceSq(viewer.GetPosition(), GetPosition()) > 14400) return false;
        if (IsLightSource()) return true;
        PPERequester_CameraNV nightVision = PPERequester_CameraNV.Cast(PPERequesterBank.GetRequester(PPERequesterBank.REQ_CAMERANV));
        return nightVision && nightVision.PredateCanSeeInfrared();
    }

    protected void UpdateEmitter()
    {
        Weapon_Base weapon = Weapon_Base.Cast(GetHierarchyParent());
        Man holder = GetHierarchyRootPlayer();
        if (!weapon || !holder || !holder.IsAlive() || holder.GetHumanInventory().GetEntityInHands() != weapon || !ViewerCanSeeEmitter())
        {
            DeleteEmitter();
            return;
        }

        if (m_PredateRenderedMode != m_PredateMode) DeleteEmitter();
        if (m_PredateMode == PredateWeaponModuleMode.VISIBLE_LASER || m_PredateMode == PredateWeaponModuleMode.INFRARED_LASER)
        {
            UpdateLaser(holder, weapon);
            return;
        }
        if (m_PredateEmitter) return;
        m_PredateEmitter = ScriptedLightBase.CreateLight(PredateModuleSpot);
        if (!m_PredateEmitter) return;
        m_PredateEmitter.AttachOnMemoryPoint(this, "beamStart", "beamEnd");
        if (m_PredateMode == PredateWeaponModuleMode.WHITE_LIGHT)
        {
            m_PredateEmitter.SetRadiusTo(45);
            m_PredateEmitter.SetSpotLightAngle(50);
            m_PredateEmitter.SetBrightnessTo(6);
            m_PredateEmitter.SetDiffuseColor(0.95, 0.88, 0.8);
        }
        m_PredateRenderedMode = m_PredateMode;
    }

    protected void UpdateLaser(Man holder, Weapon_Base weapon)
    {
        vector start, direction;
        // The first-person camera and the external model use different poses.
        if (holder == GetGame().GetPlayer())
        {
            direction = GetGame().GetCurrentCameraDirection();
            start = GetGame().GetCurrentCameraPosition() + direction * 0.15;
        }
        else if (!LaserRay(start, direction))
        {
            DeleteEmitter();
            return;
        }
        vector hit, normal;
        int component;
        bool surface = DayZPhysics.RaycastRV(start, start + direction * 90, hit, normal, component, null, weapon, holder, false, false, ObjIntersectFire, 0) && vector.DistanceSq(start, hit) >= 0.0001;
        m_PredateLaserHit = surface;
        m_PredateRenderedMode = m_PredateMode;
        bool infrared = m_PredateMode == PredateWeaponModuleMode.INFRARED_LASER;
        if (infrared)
        {
            vector end = start + direction * 20;
            if (surface) end = hit - direction * 0.025;
            UpdateInfraredBeam(holder, start, direction, end);
        }
        if (!surface)
        {
            ClearLight();
            return;
        }
        vector mark = hit - direction * 0.025;
        m_PredateLaserPoint = mark;
        if (!m_PredateEmitter)
        {
            m_PredateEmitter = ScriptedLightBase.CreateLight(PredateLaserDot, mark);
            if (!m_PredateEmitter) return;
            if (infrared) m_PredateEmitter.SetDiffuseColor(0.75, 0.9, 1);
        }
        m_PredateEmitter.SetPosition(mark);
    }

    protected void UpdateInfraredBeam(Man holder, vector start, vector direction, vector end)
    {
        ClearInfraredBeam();
        if (holder == GetGame().GetPlayer())
        {
            PlayerBase player = PlayerBase.Cast(holder);
            DayZPlayerCamera camera = player.GetCurrentPlayerCamera();
            if (!camera || camera.GetCameraName() != "DayZPlayerCamera1stPerson" || player.IsInOptics() || player.IsInIronsights()) return;

            vector rail, railDirection;
            if (LaserRay(rail, railDirection) && vector.Dot(rail - start, direction) > 0.15 && vector.DistanceSq(start, rail) < 1)
                start = rail;
            else
                start = start + direction * 0.55 + Vector(direction[2], 0, -direction[0]) * 0.12 - "0 0.10 0";
        }
        if (vector.Dot(end - start, direction) <= 0.05) return;

        // Use the scene's depth buffer so the beam can go behind foreground geometry.
        vector points[2];
        points[0] = start;
        points[1] = end;
        m_PredateInfraredBeam = Shape.CreateLines(0xB8D9F3FF, ShapeFlags.TRANSP | ShapeFlags.NOZWRITE, points, 2);
    }
#endif
}

#ifndef SERVER
class PredateModuleSpot extends SpotLightBase
{
    void PredateModuleSpot()
    {
        SetVisibleDuringDaylight(true);
        SetCastShadow(true);
        SetRadiusTo(32);
        SetSpotLightAngle(36);
        SetBrightnessTo(3.0);
        SetDiffuseColor(0.78, 0.9, 1);
    }
}

class PredateLaserDot extends PointLightBase
{
    void PredateLaserDot()
    {
        SetVisibleDuringDaylight(true);
        SetCastShadow(false);
        SetRadiusTo(0.12);
        SetBrightnessTo(2.5);
        SetDiffuseColor(1, 0.06, 0.04);
    }
}
#endif
