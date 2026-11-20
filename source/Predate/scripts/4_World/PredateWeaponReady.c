modded class PlayerBase
{
    bool m_PredateAutoRaiseReported;
    bool m_PredateOwnRaiseOverride;
    float m_PredateInteractionTime;
    bool m_PredateInteractionTap;
    bool m_PredateInteractionPreview;
    bool m_PredateInteractInputDown;
    float m_PredateRaiseDiagnosticTimer;
    bool m_PredateOwnSpeedOverride;
    bool m_PredateWalkToggled;
    bool m_PredateReadyPaceReported;
    ref HumanMovementState m_PredateReadyMovement = new HumanMovementState;
    vector m_PredatePreviousPosition;
    float m_PredateTravelTime;
    float m_PredateTravelDistance;
    bool m_PredateSamplingTravel;
    bool m_PredateTravelReported;

    override void CommandHandler(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
    {
        bool autoRaise;
        m_PredateInteractInputDown = false;
        // Run on the owning client and server. Expansion retains control of AI.
        if (!eAIBase.Cast(this) && (GetGame().IsServer() || GetGame().GetPlayer() == this))
        {
            if (GetInputInterface().SyncedPress_ID(UAWalkRunToggle))
                m_PredateWalkToggled = !m_PredateWalkToggled;
            if (PredateShouldAutoRaise(pCurrentCommandID))
            {
                PredateReadInteraction(pDt);
                autoRaise = m_PredateInteractionTime <= 0;
                if (autoRaise) PredateReadyAnimations.Apply();
                GetInputController().OverrideRaise(HumanInputControllerOverrideType.ENABLED, autoRaise);
                m_PredateOwnRaiseOverride = true;
            }
            else if (m_PredateOwnRaiseOverride)
            {
                GetInputController().OverrideRaise(HumanInputControllerOverrideType.DISABLED, false);
                m_PredateOwnRaiseOverride = false;
                m_PredateInteractionTime = 0;
                m_PredateInteractionTap = false;
            }
            PredateUpdateReadyPace(autoRaise);
        }
        super.CommandHandler(pDt, pCurrentCommandID, pCurrentCommandFinished);
        if (autoRaise && IsRaised() && !m_PredateAutoRaiseReported)
        {
            Print("[Predate] Automatic firearm ready stance confirmed.");
            m_PredateAutoRaiseReported = true;
        }
        if (!m_PredateAutoRaiseReported && GetGame().GetPlayer() == this)
        {
            m_PredateRaiseDiagnosticTimer += pDt;
            if (m_PredateRaiseDiagnosticTimer >= 5)
            {
                m_PredateRaiseDiagnosticTimer = 0;
                Print("[Predate] Ready stance check: command=" + pCurrentCommandID + "; hands=" + GetHumanInventory().GetEntityInHands() + "; eligible=" + autoRaise + "; raised=" + IsRaised());
            }
        }
        HumanCommandMove move = GetCommand_Move();
        if (m_PredateOwnSpeedOverride && move && move.GetCurrentMovementSpeed() >= 1.9 && !m_PredateReadyPaceReported)
        {
            Print("[Predate] Raised firearm jog confirmed: movement gait=" + move.GetCurrentMovementSpeed());
            m_PredateReadyPaceReported = true;
        }
        if (!m_PredateTravelReported && m_PredateOwnSpeedOverride && move && move.GetCurrentMovementSpeed() >= 1.99 && IsRaised())
        {
            vector position = GetPosition();
            if (m_PredateSamplingTravel)
            {
                vector difference = position - m_PredatePreviousPosition;
                difference[1] = 0;
                if (difference.Length() < pDt * 10)
                {
                    m_PredateTravelDistance += difference.Length();
                    m_PredateTravelTime += pDt;
                }
                if (m_PredateTravelTime >= 2)
                {
                    Print("[Predate] Raised firearm measured travel speed=" + (m_PredateTravelDistance / m_PredateTravelTime) + " m/s; reference forward jog=3.652664 m/s.");
                    m_PredateTravelReported = true;
                }
            }
            m_PredatePreviousPosition = position;
            m_PredateSamplingTravel = true;
        }
        else
        {
            m_PredateSamplingTravel = false;
            m_PredateTravelTime = 0;
            m_PredateTravelDistance = 0;
        }
    }

    void PredateReadInteraction(float seconds)
    {
        UAInterface input = GetInputInterface();
        bool pressed = input.SyncedPress_ID(UAAction) || input.SyncedValue_ID(UAAction) > 0;
        bool clicked = input.SyncedClick_ID(UAAction);
        if (GetGame().GetPlayer() == this)
        {
            UAInput interact = GetUApi().GetInputByID(UAAction);
            pressed = pressed || interact.LocalPress() || interact.LocalValue() > 0;
            clicked = clicked || interact.LocalClick();
        }
        PredateUpdateInteraction(seconds, pressed, clicked);
    }

    void PredateUpdateInteraction(float seconds, bool pressed, bool clicked)
    {
        m_PredateInteractInputDown = pressed || clicked;
        if (pressed || clicked) m_PredateInteractionTime = 0.65;
        else m_PredateInteractionTime = Math.Max(0, m_PredateInteractionTime - seconds);
        if (clicked) m_PredateInteractionTap = true;
        else if (m_PredateInteractionTime <= 0) m_PredateInteractionTap = false;
    }

    bool PredateMayPreviewInteraction()
    {
        return m_PredateOwnRaiseOverride && IsRaised() && !IsInOptics() && !IsInIronsights() && !GetGame().GetUIManager().GetMenu();
    }

    void PredateUpdateReadyPace(bool autoRaise)
    {
        GetMovementState(m_PredateReadyMovement);
        int stance = m_PredateReadyMovement.m_iStanceIdx;
        bool standing = stance == DayZPlayerConstants.STANCEIDX_ERECT || stance == DayZPlayerConstants.STANCEIDX_RAISEDERECT;
        float sideways = GetInputInterface().SyncedValue_ID(UAMoveRight) - GetInputInterface().SyncedValue_ID(UAMoveLeft);
        float forward = GetInputInterface().SyncedValue_ID(UAMoveForward) - GetInputInterface().SyncedValue_ID(UAMoveBack);
        float inputMagnitude = Math.Clamp(Math.Sqrt(sideways * sideways + forward * forward), 0, 1);
        bool walk = m_PredateWalkToggled || GetInputInterface().SyncedValue_ID(UAWalkRunTemp) > 0;
        if (autoRaise && standing && !walk && inputMagnitude > 0.01)
        {
            // Use the regular jog gait (2), rather than the weapon-up walk (1).
            // Keep analogue input magnitude and native direction/collision handling.
            GetInputController().OverrideMovementSpeed(HumanInputControllerOverrideType.ENABLED, 2 * inputMagnitude);
            m_PredateOwnSpeedOverride = true;
        }
        else if (m_PredateOwnSpeedOverride)
        {
            GetInputController().OverrideMovementSpeed(HumanInputControllerOverrideType.DISABLED, 0);
            m_PredateOwnSpeedOverride = false;
        }
    }

    bool PredateShouldAutoRaise(int commandID)
    {
        if (GetGame().IsMissionMainMenu()) return false;
        if (!IsAlive() || IsUnconscious() || commandID != DayZPlayerConstants.COMMANDID_MOVE) return false;
        Weapon_Base weapon = Weapon_Base.Cast(GetHumanInventory().GetEntityInHands());
        if (!weapon || weapon.IsDamageDestroyed() || IsRestrained() || IsSurrendered() || IsEmotePlaying()) return false;
        if (IsFighting() || IsClimbing() || IsClimbingLadder() || IsSwimming() || IsFalling() || IsInVehicle()) return false;
        if (IsMapOpen() || GetThrowing().IsThrowingModeEnabled()) return false;
        if (GetDayZPlayerInventory().IsProcessing()) return false;
        if (GetGame().IsClient() && GetGame().GetPlayer() == this && GetGame().GetUIManager().GetMenu()) return false;
        if (!GetWeaponManager()) return false;
        if (!GetActionManager()) return false;
        if (GetWeaponManager().IsRunning() || GetActionManager().GetRunningAction()) return false;
        // Lower for sprint input so the raised stance cannot prevent sprinting.
        if (GetInputInterface().SyncedValue_ID(UATurbo) > 0) return false;
        return true;
    }
}

modded class DayZPlayerMeleeFightLogic_LightHeavy
{
    override bool CanFight()
    {
        // The default interact and firearm-melee bindings both use F. Give
        // the interact input this command frame so the rifle can lower.
        if (m_Player.m_PredateInteractInputDown) return false;
        return super.CanFight();
    }
}
