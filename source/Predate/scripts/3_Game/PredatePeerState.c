// Optional roster foundation. A peer owns its record, native group and evidence.
class PredatePeerState
{
    ref PredateOperatorRecord Operator = new PredateOperatorRecord;
    vector Start;
    bool HasVitals;
    float Health;
    float Blood;
    float Shock;
    ref PredateObservationMemory Memory = new PredateObservationMemory;
    ref array<ref PredateObservation> PendingSounds = new array<ref PredateObservation>;
    ref PredateSignalTrack Receiver = new PredateSignalTrack;
    ref PredateSignalTrack PlayerReceiver = new PredateSignalTrack;
    ref PredateSignatureActivity SignatureActivity = new PredateSignatureActivity;
    ref PredateHunterState Hunter = new PredateHunterState;
    float InterceptedUntil;

    bool Validate(float now)
    {
        if (!Operator || !Memory || !PendingSounds || !Receiver || !PredateInformationMath.FiniteVector(Start)) return false;
        if (Operator.LifeState != "alive" && Operator.LifeState != "defeated" && Operator.LifeState != "unspawned") return false;
        if (Operator.Deployment < 1 || !PredateInformationMath.Finite(Operator.RespawnAt) || Operator.RespawnAt < 0) return false;
        if (!Operator.Supplies || !Operator.Supplies.Valid(now)) return false;
        if (Memory.OwnerId != Operator.Id || Receiver.OwnerId != Operator.Id) return false;
        if (!Memory.Validate(now) || !Receiver.Validate(now) || PendingSounds.Count() > 8) return false;
        if (Receiver.SubjectId != "player-01" && PredateProtocol.OperatorBit(Receiver.SubjectId) == 0) return false;
        if (!PlayerReceiver || PlayerReceiver.OwnerId != "player-01" || PlayerReceiver.SubjectId != Operator.Id || !PlayerReceiver.Validate(now)) return false;
        if (!Hunter || !Hunter.Validate(now, Operator.Id) || !SignatureActivity || !PredateInformationMath.Finite(SignatureActivity.Energy) || SignatureActivity.Energy < 0 || SignatureActivity.Energy > 1) return false;
        if (!PredateInformationMath.Finite(InterceptedUntil) || InterceptedUntil < 0) return false;
        if (HasVitals && (!PredateInformationMath.Finite(Health) || !PredateInformationMath.Finite(Blood) || !PredateInformationMath.Finite(Shock))) return false;
        foreach (PredateObservation sound : PendingSounds)
        {
            if (!sound || !sound.Valid(Operator.Id, now + 3)) return false;
            if (sound.Source != PredateObservationSource.AUDIBLE && sound.Source != PredateObservationSource.IMPACT) return false;
        }
        return true;
    }
}

class PredatePeerRosterSave
{
    int Schema = 2;
    ref array<ref PredatePeerState> Peers = new array<ref PredatePeerState>;
}
