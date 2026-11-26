class PredatePlayerLifeEvents
{
    static ref ScriptInvoker Death = new ScriptInvoker;
    static ref ScriptInvoker LocalDeath = new ScriptInvoker;
}

modded class EmoteManager
{
    override bool CanPlayEmote(int id)
    {
        if (id == EmoteConstants.ID_EMOTE_SUICIDE && IsCLIParam("predateSessionId")) return false;
        return super.CanPlayEmote(id);
    }
}

modded class DayZPlayerImplement
{
#ifndef SERVER
    protected bool m_PredateSimpleDeath;

    protected bool PredateOwnDeath()
    {
        if (GetGame().GetPlayer() != this || GetGame().GetMissionState() != DayZGame.MISSION_STATE_GAME) return false;
        PlayerBase player = PlayerBase.Cast(this);
        return GetDayZGame().m_PredateClientAgent || (player && player.m_PredateReceivedState);
    }

    override void OnSyncJuncture(int pJunctureID, ParamsReadContext pCtx)
    {
        bool waitingForDeath = pJunctureID == DayZPlayerSyncJunctures.SJ_DEATH && m_DeathAnimType == -2;
        super.OnSyncJuncture(pJunctureID, pCtx);
        if (waitingForDeath && m_DeathAnimType != -2 && PredateOwnDeath())
            PredatePlayerLifeEvents.LocalDeath.Invoke(this);
    }

    override bool HandleDeath(int pCurrentCommandID)
    {
        if (!PredateOwnDeath() || (IsAlive() && m_DeathAnimType == -2)) return super.HandleDeath(pCurrentCommandID);
        if (!m_PredateSimpleDeath)
        {
            m_PredateSimpleDeath = true;
            PredatePlayerLifeEvents.LocalDeath.Invoke(this);
            PhysicsSetSimpleDeath(true);
            GetGame().GetWorld().SetVoiceOn(false, false);
        }
        return true;
    }

    override void ShowDeadScreen(bool show, float duration)
    {
        if (show && PredateOwnDeath())
        {
            StopDeathDarkeningEffect();
            PredatePlayerLifeEvents.LocalDeath.Invoke(this);
            return;
        }
        super.ShowDeadScreen(show, duration);
    }
#endif

    override int CameraHandler(int pCameraMode)
    {
        // Older launcher builds can still create a third-person server config.
        if (GetGame().GetPlayer() == this)
        {
            PlayerBase player = PlayerBase.Cast(this);
            if (GetDayZGame().m_PredateClientAgent || (player && player.m_PredateReceivedState))
                SetIsInThirdPerson(false);
        }
        return super.CameraHandler(pCameraMode);
    }
}

modded class PlayerBase
{
    static PlayerBase s_PredateTrackingRequester;
    ref PredateState m_PredateState;
    int m_PredateRequest;
    int m_PredateTrackingRequest;
    int m_PredateTrackingChoice;
    string m_PredateRequestedOperator = "raven-01";
    bool m_PredateReceivedState;
    bool m_PredateSessionQuitRequested;
    bool m_PredateSuicideRequested;
    bool m_PredateStagedForDeployment;
    int m_PredateScoringLife;
    string m_PredateScoreKiller;
    bool m_PredateScoreSuicide;

    bool m_PredateDeathRecorded;
    bool m_PredateNativeInsertion;
    bool m_PredateSpawnRejected;
    bool m_PredateHasDeploymentPosition;
    vector m_PredateDeploymentPosition;
    ref array<int> m_PredateKillNotices = {};
    ref array<int> m_PredateLostSignalNotices = {};
    string m_PredateFootprintNotice;
    vector m_PredateFootprintLook;
    int m_PredateFootprintLookSentAt;
    int m_PredateFootprintLookReceivedAt;
    bool m_PredateSupplyNotice;
    string m_PredateModuleMessage;
    int m_PredateModuleMessageTime;
    int m_PredateModuleMessageRevision;

    void PredateWeaponModuleMessage(string message)
    {
        if (GetGame().IsMultiplayer() && GetGame().IsServer())
        {
            PlayerIdentity identity = GetIdentity();
            if (!identity) return;
            ScriptRPC rpc = new ScriptRPC;
            rpc.Write(message);
            rpc.Send(this, PredateProtocol.WEAPON_MODULE_LOG, true, identity);
        }
        else
        {
            m_PredateModuleMessage = message;
            m_PredateModuleMessageTime = GetGame().GetTime();
            m_PredateModuleMessageRevision++;
        }
    }

    override void EEKilled(Object killer)
    {
        bool human = !eAIBase.Cast(this);
        // Set the flag before Expansion processes death and drops the held item.
        if (human) Expansion_SetCanBeLooted(false);
        if (GetGame().IsServer() && GetIdentity())
        {
            eAIBase rival = eAIBase.Cast(PredateOperatorDeaths.Attacker(killer));
            if (rival && rival.m_PredateOperatorRecord) m_PredateScoreKiller = rival.m_PredateOperatorRecord.Id;
            PredatePlayerLifeEvents.Death.Invoke(this);
        }
        super.EEKilled(killer);
        if (human && GetGame().IsServer()) PredateSealCorpseInventory();
    }

    bool PredateIsHumanCorpse()
    {
        return !IsAlive() && !eAIBase.Cast(this);
    }

    override bool IsInventoryVisible()
    {
        if (PredateIsHumanCorpse()) return false;
        return super.IsInventoryVisible();
    }

    override bool CanBeSkinned()
    {
        if (PredateIsHumanCorpse()) return false;
        return super.CanBeSkinned();
    }

    override void AfterStoreLoad()
    {
        super.AfterStoreLoad();
        if (!PredateIsHumanCorpse()) return;
        Expansion_SetCanBeLooted(false);
        PredateSealCorpseInventory();
    }

    void PredateSealCorpseInventory()
    {
        // Also mark the items themselves, including contents of clothing.
        // A direct item interaction must obey the same rule as the corpse UI.
        array<EntityAI> items = {};
        GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
        foreach (EntityAI item : items)
            if (item && item != this) ExpansionItemBaseModule.SetLootable(item, false);
        GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
    }

    void PredateRequest(int request, string operatorId = "raven-01")
    {
        if (!GetGame().IsClient()) return;
        Print("[Predate] Sending receiver request " + request);
        ScriptRPC rpc = new ScriptRPC;
        rpc.Write(PredateProtocol.VERSION);
        rpc.Write(operatorId);
        rpc.Write(request);
        rpc.Send(this, PredateProtocol.REQUEST, true);
    }

    void PredateSendFootprintLook()
    {
        int now = GetGame().GetTime();
        if (!GetGame().IsClient() || now < m_PredateFootprintLookSentAt + 250) return;
        m_PredateFootprintLookSentAt = now;
        ScriptRPC rpc = new ScriptRPC;
        rpc.Write(GetGame().GetCurrentCameraDirection());
        rpc.Send(this, PredateProtocol.FOOTPRINT_LOOK, false);
    }

    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);
        if (rpc_type == PredateProtocol.SIGNAL_LOST && GetGame().IsClient() && this == GetGame().GetPlayer())
        {
            int lostSlot;
            if (ctx.Read(lostSlot) && PredateProtocol.OperatorAt(lostSlot) != "" && m_PredateLostSignalNotices.Find(lostSlot) < 0)
                m_PredateLostSignalNotices.Insert(lostSlot);
            return;
        }
        if (rpc_type == PredateProtocol.WEAPON_MODULE_LOG && GetGame().IsClient() && this == GetGame().GetPlayer())
        {
            string message;
            if (ctx.Read(message)) PredateWeaponModuleMessage(message);
            return;
        }
        if (rpc_type == PredateProtocol.FOOTPRINT_LOOK && GetGame().IsServer())
        {
            if (!sender || !GetIdentity() || sender.GetId() != GetIdentity().GetId() || !IsAlive() || IsUnconscious()) return;
            int now = GetGame().GetTime();
            if (now < m_PredateFootprintLookReceivedAt + 150) return;
            vector look;
            if (!ctx.Read(look) || !PredateInformationMath.FiniteVector(look) || look.LengthSq() < 0.8 || look.LengthSq() > 1.2) return;
            m_PredateFootprintLook = look.Normalized();
            m_PredateFootprintLookReceivedAt = now;
            return;
        }
        if (rpc_type == PredateProtocol.SESSION_REQUEST && GetGame().IsServer())
        {
            if (!sender || !GetIdentity()) return;
            if (sender.GetId() != GetIdentity().GetId()) return;
            int sessionVersion;
            string sessionId;
            string expectedSession;
            if (!GetGame().CommandlineGetParam("predateSessionId", expectedSession)) return;
            if (!ctx.Read(sessionVersion) || sessionVersion != 1) return;
            if (!ctx.Read(sessionId) || sessionId != expectedSession) return;
            m_PredateSessionQuitRequested = true;
            return;
        }
        if (rpc_type == PredateProtocol.SESSION_ACK && GetGame().IsClient())
        {
            string ackId;
            string clientSession;
            if (!GetGame().CommandlineGetParam("predateSessionId", clientSession)) return;
            if (ctx.Read(ackId) && ackId == clientSession) GetDayZGame().m_PredateSessionQuitAck = true;
            return;
        }
        if (rpc_type == PredateProtocol.REQUEST && GetGame().IsServer())
        {
            if (!sender || !GetIdentity() || sender.GetId() != GetIdentity().GetId()) return;
            int request;
            int version;
            string operatorId;
            if (!ctx.Read(version) || version != PredateProtocol.VERSION) return;
            if (!ctx.Read(operatorId)) return;
            if (ctx.Read(request) && PredateProtocol.AcceptRequest(version, operatorId, request))
            {
                if (request == PredateProtocol.REQUEST_SUICIDE)
                {
                    m_PredateSuicideRequested = true;
                    return;
                }
                if (request == 1 || request == 2)
                {
                    m_PredateTrackingRequest = request;
                    m_PredateRequestedOperator = operatorId;
                    s_PredateTrackingRequester = this;
                }
                else m_PredateRequest = request;
                Print("[Predate] Received receiver request " + request);
            }
        }
        else if (rpc_type == PredateProtocol.STATE && GetGame().IsClient())
        {
            PredateState state = new PredateState;
            int stateVersion;
            if (!ctx.Read(stateVersion) || stateVersion != PredateProtocol.VERSION) return;
            bool ok = ctx.Read(state.OperatorId);
            ok = ok && PredateProtocol.OperatorSlot(state.OperatorId) >= 0;
            ok = ok && ctx.Read(state.Development);
            ok = ok && ctx.Read(state.CanDeploy);
            ok = ok && ctx.Read(state.Signature);
            ok = ok && state.Signature == PredateProtocol.SignatureFor(state.OperatorId);
            ok = ok && ctx.Read(state.Available);
            ok = ok && ctx.Read(state.Locked);
            ok = ok && ctx.Read(state.TrackingChoice);
            ok = ok && ctx.Read(state.HasPulse);
            ok = ok && ctx.Read(state.CloseApproach);
            ok = ok && ctx.Read(state.Bearing);
            if (ok && state.HasPulse) ok = PredateBearingPulse.Valid(state.Bearing);
            ok = ok && ctx.Read(state.Age);
            ok = ok && ctx.Read(state.NextPulse);
            ok = ok && ctx.Read(state.Message);
            ok = ok && ctx.Read(state.HasPulseOrigin);
            ok = ok && ctx.Read(state.PulseOrigin);
            ok = ok && ctx.Read(state.BearingError);
            ok = ok && ctx.Read(state.Intercepted);
            ok = ok && PredateReadKills(ctx, state.ConfirmedKills);
            ok = ok && ctx.Read(state.SuppliesRefreshed);
            ok = ok && ctx.Read(state.NeedsRedeploy);
            if (ok && state.NeedsRedeploy) ok = ctx.Read(state.DeploymentSpawnRejected);
            ok = ok && PredateReadBearingHistory(ctx, state);
            ok = ok && PredateReadPulses(ctx, state);
            ok = ok && ctx.Read(state.AvailableOperators);
            ok = ok && state.AvailableOperators >= -1 && state.AvailableOperators <= PredateProtocol.MAX_OPERATORS;
            if (ok && state.AvailableOperators == -1) ok = state.OperatorId == "raven-01";
            ok = ok && ctx.Read(state.FootprintClue);
            ok = ok && state.FootprintClue >= 0 && state.FootprintClue <= 2;
            if (ok && state.FootprintClue > 0)
            {
                ok = ctx.Read(state.FootprintGround) && ctx.Read(state.FootprintHeading);
                ok = ok && PredateInformationMath.FiniteVector(state.FootprintGround) && state.FootprintHeading >= 0 && state.FootprintHeading < 360;
            }
            ok = ok && ctx.Read(state.FootprintsEnabled);
            ok = ok && ctx.Read(state.OwnFootprint);
            if (ok && state.OwnFootprint)
            {
                ok = ctx.Read(state.OwnFootprintGround) && ctx.Read(state.OwnFootprintHeading) && ctx.Read(state.InspectingOwnFootprint);
                ok = ok && PredateInformationMath.FiniteVector(state.OwnFootprintGround) && state.OwnFootprintHeading >= 0 && state.OwnFootprintHeading < 360;
            }
            string footprintNotice;
            ok = ok && ctx.Read(footprintNotice);
            ok = ok && ctx.Read(state.HasDeploymentPosition);
            if (ok && state.HasDeploymentPosition)
                ok = ctx.Read(state.DeploymentPosition) && PredateInformationMath.FiniteVector(state.DeploymentPosition);
            ok = ok && ctx.Read(state.LifeScore);
            if (ok)
            {
                if (footprintNotice != "") m_PredateFootprintNotice = footprintNotice;
                m_PredateState = state;
                foreach (int slot : state.ConfirmedKills)
                    if (m_PredateKillNotices.Find(slot) < 0) m_PredateKillNotices.Insert(slot);
                m_PredateSupplyNotice = m_PredateSupplyNotice || state.SuppliesRefreshed;
                if (!m_PredateReceivedState)
                {
                    Print("[Predate] Receiver state synchronized; protocol=" + stateVersion + "; operator=" + state.OperatorId + "; target available=" + state.Available + "; locked=" + state.Locked + "; sample age=" + state.Age + "; next pulse=" + state.NextPulse);
                    m_PredateReceivedState = true;
                }
            }
            else Print("[Predate] Receiver state packet could not be decoded.");
        }
    }

    bool PredateReadKills(ParamsReadContext ctx, array<int> kills)
    {
        int count;
        if (!ctx.Read(count) || count < 0 || count > PredateProtocol.MAX_OPERATORS) return false;
        for (int index = 0; index < count; index++)
        {
            int slot;
            if (!ctx.Read(slot) || slot < 0 || slot >= PredateProtocol.MAX_OPERATORS || kills.Find(slot) >= 0) return false;
            kills.Insert(slot);
        }
        return true;
    }

    bool PredateReadBearingHistory(ParamsReadContext ctx, PredateState state)
    {
        int count;
        if (!ctx.Read(count) || count < 0 || count > PredateProtocol.MAX_BEARING_HISTORY) return false;
        if (count > 0 && !state.CanShowHistory()) return false;
        for (int index = 0; index < count; index++)
        {
            PredateReceiverSample sample = new PredateReceiverSample;
            if (!ctx.Read(sample.Bearing) || !ctx.Read(sample.Error) || !ctx.Read(sample.Age) || !ctx.Read(sample.Origin) || !ctx.Read(sample.DistanceBand)) return false;
            if (sample.DistanceBand < 0 || sample.DistanceBand > PredateBearingPulse.MAX_BAND) return false;
            if (!PredateBearingPulse.Valid(sample.Bearing) || !PredateInformationMath.FiniteVector(sample.Origin)) return false;
            if (!PredateInformationMath.Finite(sample.Age) || sample.Age < 0 || !PredateInformationMath.Finite(sample.Error) || sample.Error < 0) return false;
            state.History.Insert(sample);
        }
        return true;
    }

    bool PredateReadPulses(ParamsReadContext ctx, PredateState state)
    {
        int count;
        if (!ctx.Read(count) || count < 0 || count > PredateProtocol.MAX_OPERATORS) return false;
        if (count > 0 && state.NeedsRedeploy) return false;
        array<string> seen = {};
        for (int index = 0; index < count; index++)
        {
            PredateBearingPulse pulse = new PredateBearingPulse;
            if (!ctx.Read(pulse.OperatorId) || !ctx.Read(pulse.Start) || !PredateBearingPulse.Valid(pulse.Start)) return false;
            if (!ctx.Read(pulse.DistanceBand) || pulse.DistanceBand < 1 || pulse.DistanceBand > PredateBearingPulse.MAX_BAND) return false;
            if (PredateProtocol.OperatorSlot(pulse.OperatorId) < 0 || seen.Find(pulse.OperatorId) >= 0) return false;
            seen.Insert(pulse.OperatorId);
            state.Pulses.Insert(pulse);
        }
        return true;
    }
}
