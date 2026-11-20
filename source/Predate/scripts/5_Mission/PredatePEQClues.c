class PredatePEQClues
{
    static void Scan(array<ref PredateHuntParticipant> participants, float now)
    {
        array<ref PredatePEQSample> samples = {};
        foreach (PredateHuntParticipant source : participants)
            PredatePEQVision.Collect(source.Actor, samples);
        if (!samples.Count()) return;

        bool dark = GetGame().GetWorld().IsNight();
        if (dark) MarkIllumination(participants, samples, now);
        foreach (PredateHuntParticipant participant : participants)
        {
            eAIBase observer = eAIBase.Cast(participant.Actor);
            if (!observer || !participant.Memory || !observer.m_PredateKnowledgeEnabled || observer.GetIsSimulationDisabled() || observer.IsUnconscious()) continue;
            RememberClue(observer, participant.Memory, samples, now, dark);
        }
    }

    protected static void MarkIllumination(array<ref PredateHuntParticipant> participants, array<ref PredatePEQSample> samples, float now)
    {
        foreach (PredateHuntParticipant participant : participants)
        {
            PlayerBase target = participant.Actor;
            target.m_PredatePEQWhiteLitUntil = 0;
            target.m_PredatePEQInfraredLitUntil = 0;
            vector point = target.GetBonePositionWS(target.GetBoneIndexByName("Neck"));
            foreach (PredatePEQSample sample : samples)
            {
                if (sample.Laser || !sample.Illuminates(target, point)) continue;
                if (sample.Infrared) target.m_PredatePEQInfraredLitUntil = now + 1;
                else target.m_PredatePEQWhiteLitUntil = now + 1;
            }
        }
    }

    protected static void RememberClue(eAIBase observer, PredateObservationMemory memory, array<ref PredatePEQSample> samples, float now, bool dark)
    {
        PredateObservation previous = memory.Latest("", PredateObservationSource.LIGHT, now);
        if (previous && now - previous.ObservedAt < 8) return;
        if (observer.m_PredateLastVisualAt >= 0 && now - observer.m_PredateLastVisualAt < 6) return;
        bool nightVision = PredatePEQVision.HasNightVision(observer);
        vector eyes = observer.GetBonePositionWS(observer.GetBoneIndexByName("Head"));
        vector nearestArea;
        float nearestRadius;
        float nearestDistance = 121 * 121;
        foreach (PredatePEQSample sample : samples)
        {
            if (sample.Holder == observer || (sample.Infrared && !nightVision)) continue;
            vector area;
            float radius;
            if (!sample.FindClue(observer, eyes, dark, area, radius)) continue;
            float distance = vector.DistanceSq(eyes, area);
            if (distance >= nearestDistance) continue;
            nearestDistance = distance;
            nearestArea = area;
            nearestRadius = radius;
        }
        if (nearestRadius == 0 || (previous && vector.DistanceSq(previous.Estimate, nearestArea) < 144)) return;

        PredateObservation clue = new PredateObservation;
        clue.ObserverId = memory.OwnerId;
        clue.EventKey = "light-" + memory.NextSerial;
        clue.Source = PredateObservationSource.LIGHT;
        clue.ObservedAt = now;
        clue.ExpiresAt = now + 36;
        clue.Confidence = 0.55;
        clue.HasPosition = true;
        clue.Estimate = nearestArea;
        clue.Radius = nearestRadius;
        memory.Add(clue, now);
    }
}
