class PredatePlayerLifeEvents
{
    static ref ScriptInvoker Death = new ScriptInvoker;
}

modded class DayZPlayerImplement
{
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
    ref PredateState m_PredateState;
    int m_PredateRequest;
    string m_PredateRequestedOperator = "raven-01";
    bool m_PredateReceivedState;
    bool m_PredateSessionQuitRequested;
    bool m_PredateStagedForDeployment;
    bool m_PredateDeathRecorded;
    bool m_PredateNativeInsertion;
    bool m_PredateSpawnRejected;
    int m_PredateKillNotices;
    bool m_PredateSupplyNotice;

    override void EEKilled(Object killer)
    {
        bool human = !eAIBase.Cast(this);
        // Set the flag before Expansion processes death and drops the held item.
        if (human) Expansion_SetCanBeLooted(false);
        if (GetGame().IsServer() && GetIdentity()) PredatePlayerLifeEvents.Death.Invoke(this);
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

    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);
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
                m_PredateRequest = request;
                m_PredateRequestedOperator = operatorId;
                Print("[Predate] Received receiver request " + request);
            }
        }
        else if (rpc_type == PredateProtocol.STATE && GetGame().IsClient())
        {
            PredateState state = new PredateState;
            int stateVersion;
            if (!ctx.Read(stateVersion) || stateVersion != PredateProtocol.VERSION) return;
            bool ok = ctx.Read(state.OperatorId);
            ok = ok && PredateProtocol.OperatorBit(state.OperatorId) != 0;
            ok = ok && ctx.Read(state.Development);
            ok = ok && ctx.Read(state.CanDeploy);
            ok = ok && ctx.Read(state.Signature);
            ok = ok && state.Signature == PredateProtocol.SignatureFor(state.OperatorId);
            ok = ok && ctx.Read(state.Available);
            ok = ok && ctx.Read(state.Locked);
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
            ok = ok && ctx.Read(state.ConfirmedKills) && state.ConfirmedKills >= 0 && state.ConfirmedKills <= PredateProtocol.ALL_OPERATORS;
            ok = ok && ctx.Read(state.SuppliesRefreshed);
            ok = ok && ctx.Read(state.NeedsRedeploy);
            if (ok && state.NeedsRedeploy) ok = ctx.Read(state.DeploymentSpawnRejected);
            ok = ok && PredateReadBearingHistory(ctx, state);
            ok = ok && PredateReadPulses(ctx, state);
            ok = ok && ctx.Read(state.AvailableOperators);
            ok = ok && state.AvailableOperators >= -1 && state.AvailableOperators <= PredateProtocol.ALL_OPERATORS;
            if (ok && state.AvailableOperators == -1) ok = state.OperatorId == "raven-01";
            ok = ok && ctx.Read(state.FootprintClue);
            ok = ok && state.FootprintClue >= 0 && state.FootprintClue <= 2;
            if (ok && state.FootprintClue > 0)
            {
                ok = ctx.Read(state.FootprintGround) && ctx.Read(state.FootprintHeading);
                ok = ok && PredateInformationMath.FiniteVector(state.FootprintGround) && state.FootprintHeading >= 0 && state.FootprintHeading < 360;
            }
            if (ok)
            {
                m_PredateState = state;
                m_PredateKillNotices = m_PredateKillNotices | state.ConfirmedKills;
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
        int seen;
        for (int index = 0; index < count; index++)
        {
            PredateBearingPulse pulse = new PredateBearingPulse;
            if (!ctx.Read(pulse.OperatorId) || !ctx.Read(pulse.Start) || !PredateBearingPulse.Valid(pulse.Start)) return false;
            if (!ctx.Read(pulse.DistanceBand) || pulse.DistanceBand < 1 || pulse.DistanceBand > PredateBearingPulse.MAX_BAND) return false;
            int bit = PredateProtocol.OperatorBit(pulse.OperatorId);
            if (bit == 0 || (seen & bit) != 0) return false;
            seen = seen | bit;
            state.Pulses.Insert(pulse);
        }
        return true;
    }
}
