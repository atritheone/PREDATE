// The dedicated server keeps its native network/event loop responsive. While
// held, immediate engine housekeeping still runs with zero elapsed script time.
// Only explicitly registered control is polled with real elapsed time.
class PredateServerPauseDriver
{
    static bool Held;
    static ref ScriptInvoker Poll = new ScriptInvoker;
}

modded class DayZGame
{
    override void OnUpdate(bool doSim, float timeslice)
    {
        if (IsDedicatedServer() && PredateServerPauseDriver.Held)
        {
            super.OnUpdate(false, 0);
            m_DeltaTime = timeslice;
            PredateServerPauseDriver.Poll.Invoke(timeslice);
            return;
        }
        super.OnUpdate(doSim, timeslice);
        // Closing releases the timer hold but deliberately retains the mission
        // pause. Continue polling its Stop command until native exit completes.
        if (IsDedicatedServer()) PredateServerPauseDriver.Poll.Invoke(timeslice);
    }

    override void OnPostUpdate(bool doSim, float timeslice)
    {
        if (IsDedicatedServer() && PredateServerPauseDriver.Held)
        {
            super.OnPostUpdate(false, 0);
            return;
        }
        super.OnPostUpdate(doSim, timeslice);
    }
}
