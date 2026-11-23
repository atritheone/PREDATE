#ifndef SERVER
modded class VONManagerImplementation
{
    override void OnEvent(EventType eventTypeId, Param params)
    {
        if (!IsCLIParam("predateSessionId")) super.OnEvent(eventTypeId, params);
    }
}
#endif
