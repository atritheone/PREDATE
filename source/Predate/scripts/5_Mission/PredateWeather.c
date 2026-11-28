class PredateWeather
{
    protected static const float FIXED_HOLD_SECONDS = 86400;

    static bool IsSupported(string preset, string terrain)
    {
        switch (preset)
        {
            case "dynamic":
            case "clear":
            case "overcast":
                return true;
            case "fog":
            case "rain":
            case "thunderstorm":
                return terrain == "chernarusplus" || terrain == "enoch";
            case "snow":
            case "blizzard":
                return terrain == "sakhal";
            case "high-wind":
                return terrain == "nasdara";
        }
        return false;
    }

    static void Apply(PredateSession session)
    {
        if (session.m_WeatherInitialized) return;
        session.m_WeatherInitialized = true;
        Weather weather = GetGame().GetWeather();
        string preset = session.m_HuntConfig.WeatherPreset;
        string terrain = GetGame().GetWorldName();
        terrain.ToLower();
        eAIBase.s_PredateSandstormAffectsVisibility = preset == "dynamic" && terrain == "nasdara";
        weather.MissionWeather(false);
        if (preset == "dynamic")
        {
            // The active WorldData supplies each terrain's native weather cycle.
            weather.SetWeatherUpdateFreeze(session.m_Paused);
            if (!Restore(weather, session.m_WorldState.WeatherSnapshot)) RecoverOldClearForecast(weather);
        }
        else
        {
            ApplyFixed(weather, preset);
        }

        if (!PredateServerDiagnostics.Enabled) return;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Report, 2000, false, preset, "startup");
        if (preset == "dynamic")
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Report, 30000, false, preset, "followup");
    }

    protected static void ApplyFixed(Weather weather, string preset)
    {
        float overcast, fog, rain, snow, distanceFog, lightning;
        float wind = 2;
        switch (preset)
        {
            case "overcast":
                overcast = 0.72;
                wind = 4;
                break;
            case "fog":
                overcast = 0.46;
                fog = 0.22;
                break;
            case "rain":
                overcast = 0.82;
                rain = 0.65;
                wind = 5;
                break;
            case "thunderstorm":
                overcast = 0.96;
                rain = 0.90;
                wind = 11;
                lightning = 1;
                break;
            case "snow":
                overcast = 0.66;
                snow = 0.55;
                wind = 4;
                break;
            case "blizzard":
                overcast = 0.91;
                snow = 0.90;
                wind = 13;
                distanceFog = 0.08;
                break;
            case "high-wind":
                overcast = 0.32;
                wind = 14;
                break;
        }

        weather.SetWeatherUpdateFreeze(true);
        Sandstorm dust = weather.GetSandstorm();
        if (dust && dust.IsActive()) dust.Stop(0, true);
        Fix(weather.GetOvercast(), overcast);
        Fix(weather.GetFog(), fog);
        weather.SetRainThresholds(0.60, 1, 0);
        weather.SetSnowfallThresholds(0.30, 1, 0);
        Fix(weather.GetRain(), rain);
        Fix(weather.GetSnowfall(), snow);
        // A height density of zero leaves low ground fog even at zero distance density.
        weather.SetDynVolFogHeightDensity(1);
        weather.SetDynVolFogDistanceDensity(distanceFog);
        weather.SetStorm(lightning, 0.90, 30);
        Fix(weather.GetWindMagnitude(), wind);
        weather.GetWindDirection().Set(0, 0, FIXED_HOLD_SECONDS);
    }

    protected static void Fix(WeatherPhenomenon phenomenon, float value)
    {
        phenomenon.SetLimits(value, value);
        phenomenon.SetForecastChangeLimits(0, 0);
        phenomenon.Set(value, 0, FIXED_HOLD_SECONDS);
    }

    protected static bool CollectPhenomena(Weather weather, array<WeatherPhenomenon> phenomena)
    {
        if (!weather || !phenomena) return false;
        phenomena.Insert(weather.GetOvercast());
        phenomena.Insert(weather.GetFog());
        phenomena.Insert(weather.GetRain());
        phenomena.Insert(weather.GetSnowfall());
        phenomena.Insert(weather.GetWindMagnitude());
        phenomena.Insert(weather.GetWindDirection());
        foreach (WeatherPhenomenon phenomenon : phenomena)
            if (!phenomenon) return false;
        return true;
    }

    protected static PredateWeatherSnapshot Capture(Weather weather, bool holdCurrent = false)
    {
        array<WeatherPhenomenon> phenomena = {};
        if (!CollectPhenomena(weather, phenomena)) return null;
        PredateWeatherSnapshot snapshot = new PredateWeatherSnapshot;
        Sandstorm dust = weather.GetSandstorm();
        snapshot.HadSandstorm = dust && dust.IsActive();
        snapshot.DistanceFog = weather.GetDynVolFogDistanceDensity();
        snapshot.HeightFog = weather.GetDynVolFogHeightDensity();
        snapshot.HeightBias = weather.GetDynVolFogHeightBias();
        foreach (WeatherPhenomenon phenomenon : phenomena)
        {
            PredateWeatherForecast forecast = new PredateWeatherForecast;
            forecast.Actual = phenomenon.GetActual();
            forecast.Target = phenomenon.GetForecast();
            forecast.Remaining = Math.Clamp(phenomenon.GetNextChange(), 0, 100000);
            snapshot.Forecasts.Insert(forecast);
        }
        if (holdCurrent && !snapshot.HadSandstorm)
        {
            // The engine continues an interpolation after forecast computation is frozen.
            for (int i = 0; i < phenomena.Count(); i++)
                phenomena[i].Set(snapshot.Forecasts[i].Actual, 0, FIXED_HOLD_SECONDS);
            ApplyVolumetricFog(weather, snapshot);
        }
        return snapshot;
    }

    static void CaptureForSave(PredateSession session)
    {
        if (session.m_HuntConfig.WeatherPreset != "dynamic")
        {
            session.m_WorldState.WeatherSnapshot = null;
            return;
        }
        if (session.m_PausedWeather)
            session.m_WorldState.WeatherSnapshot = session.m_PausedWeather;
        else
            session.m_WorldState.WeatherSnapshot = Capture(GetGame().GetWeather());
    }

    static void SetHeld(PredateSession session, bool held)
    {
        if (!session.m_WeatherInitialized || session.m_HuntConfig.WeatherPreset != "dynamic") return;
        Weather weather = GetGame().GetWeather();
        if (!weather) return;
        if (held)
        {
            if (!session.m_PausedWeather) session.m_PausedWeather = Capture(weather, true);
        }
        else
        {
            Restore(weather, session.m_PausedWeather);
            session.m_PausedWeather = null;
        }
        weather.SetWeatherUpdateFreeze(held);
    }

    protected static bool Valid(PredateWeatherSnapshot snapshot, int count)
    {
        if (!snapshot || snapshot.HadSandstorm || !snapshot.Forecasts || snapshot.Forecasts.Count() != count) return false;
        if (!PredateInformationMath.Finite(snapshot.DistanceFog) || snapshot.DistanceFog < 0 || snapshot.DistanceFog > 1) return false;
        if (!PredateInformationMath.Finite(snapshot.HeightFog) || snapshot.HeightFog < 0 || snapshot.HeightFog > 1) return false;
        if (!PredateInformationMath.Finite(snapshot.HeightBias)) return false;
        foreach (PredateWeatherForecast forecast : snapshot.Forecasts)
        {
            if (!forecast || !PredateInformationMath.Finite(forecast.Actual) || !PredateInformationMath.Finite(forecast.Target)) return false;
            if (!PredateInformationMath.Finite(forecast.Remaining) || forecast.Remaining < 0 || forecast.Remaining > 100000) return false;
        }
        return true;
    }

    protected static bool Restore(Weather weather, PredateWeatherSnapshot snapshot)
    {
        if (!weather || !snapshot) return false;
        Sandstorm dust = weather.GetSandstorm();
        // The native controller owns the position and motion of a sandstorm front.
        if (dust && dust.IsActive()) return false;
        array<WeatherPhenomenon> phenomena = {};
        if (!CollectPhenomena(weather, phenomena) || !Valid(snapshot, phenomena.Count())) return false;
        for (int i = 0; i < phenomena.Count(); i++)
        {
            PredateWeatherForecast forecast = snapshot.Forecasts[i];
            WeatherPhenomenon phenomenon = phenomena[i];
            phenomenon.Set(forecast.Actual, 0, 0);
            if (forecast.Actual == forecast.Target)
                phenomenon.Set(forecast.Target, 0, forecast.Remaining);
            else
                phenomenon.Set(forecast.Target, Math.Max(0.1, forecast.Remaining), 0);
        }
        ApplyVolumetricFog(weather, snapshot);
        return true;
    }

    protected static void ApplyVolumetricFog(Weather weather, PredateWeatherSnapshot snapshot)
    {
        weather.SetDynVolFogDistanceDensity(snapshot.DistanceFog);
        weather.SetDynVolFogHeightDensity(snapshot.HeightFog);
        weather.SetDynVolFogHeightBias(snapshot.HeightBias);
    }

    protected static void RecoverOldClearForecast(Weather weather)
    {
        Sandstorm dust = weather.GetSandstorm();
        if (dust && dust.IsActive()) return;
        WeatherPhenomenon overcast = weather.GetOvercast();
        // Older saves can carry the previous Clear implementation's long forecast hold.
        if (overcast.GetForecast() == 0 && overcast.GetNextChange() > 3600)
            overcast.SetNextChange(1);
    }

    protected static void Report(string preset, string phase)
    {
        Weather weather = GetGame().GetWeather();
        Sandstorm dust = weather.GetSandstorm();
        bool sandstorm = dust && dust.IsActive();
        bool lightning = preset == "thunderstorm";
        float nextOvercast = weather.GetOvercast().GetNextChange();
        float now = weather.GetTime();
        string message = "[PredateWeather] phase=" + phase + " preset=" + preset + " frozen=" + weather.GetWeatherUpdateFrozen();
        message += " overcast=" + weather.GetOvercast().GetActual() + " fog=" + weather.GetFog().GetActual();
        message += " rain=" + weather.GetRain().GetActual() + " snowfall=" + weather.GetSnowfall().GetActual();
        message += " wind=" + weather.GetWindMagnitude().GetActual() + " distance_fog=" + weather.GetDynVolFogDistanceDensity();
        message += " lightning_preset=" + lightning + " sandstorm=" + sandstorm;
        message += " forecast_overcast=" + weather.GetOvercast().GetForecast() + " next_overcast=" + nextOvercast;
        message += " weather_time=" + now;
        Print(message);
    }
}
