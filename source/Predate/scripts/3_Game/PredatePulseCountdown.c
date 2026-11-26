class PredatePulseCountdown
{
    protected bool m_Tracking;
    protected string m_Operator;
    protected int m_Choice;
    protected float m_Deadline;
    protected int m_ClockRevision;
    protected float m_Remaining;
    protected float m_Estimate;

    void Sync(PredateState state)
    {
        if (!state.Available || !state.Locked)
        {
            m_Tracking = false;
            return;
        }

        bool changedTarget = m_Operator != state.OperatorId || m_Choice != state.TrackingChoice;
        bool changedClock = m_Deadline != state.PulseDeadline || m_ClockRevision != state.PulseClockRevision;
        if (!m_Tracking || changedTarget || changedClock)
            m_Remaining = state.NextPulse;
        m_Tracking = true;
        m_Operator = state.OperatorId;
        m_Choice = state.TrackingChoice;
        m_Deadline = state.PulseDeadline;
        m_ClockRevision = state.PulseClockRevision;
        m_Estimate = state.NextPulse;
    }

    void Advance(float seconds, bool held = false)
    {
        if (!m_Tracking || held || seconds <= 0) return;
        // Correct snapshot jitter by changing the rate slightly within a pulse.
        float correction = Math.Clamp(m_Remaining - m_Estimate, -0.2, 0.2);
        m_Remaining = Math.Max(0, m_Remaining - seconds * (1 + correction));
        m_Estimate = Math.Max(0, m_Estimate - seconds);
    }

    float Remaining() { return m_Remaining; }
}
