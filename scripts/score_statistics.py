"""Aggregate the player-facing measurements in saved deployment and encounter scores."""
from collections import Counter
from math import isfinite


def count(record, field):
    value = record.get(field)
    return value if type(value) is int and 0 <= value <= 2**53 - 1 else 0


def measurement(record, field):
    value = record.get(field)
    return value if type(value) in (int, float) and 0 <= value <= 2**53 - 1 and isfinite(value) else 0


def summarize(saves, *, lifetime_seconds=None):
    totals = Counter()
    deployment_outcomes = Counter()
    encounter_outcomes = Counter()
    deaths_by_operator = Counter()
    kills_by_operator = Counter()

    for state, lives in saves:
        totals["hunts"] += 1
        totals["recorded_deployments"] += len(lives)
        elapsed = measurement(state, "SimulationSeconds")
        life_number = count(state, "PlayerLife")
        awaiting_redeploy = state.get("PlayerAwaitingRedeploy") is True
        if not (elapsed or life_number > 1 or awaiting_redeploy):
            life_number = 0
        totals["deployments"] += max(life_number, len(lives))
        recorded_deaths = sum(life.get("EndReason") in ("Died", "KIA", "Suicide") for life in lives)
        totals["deaths"] += max(max(0, life_number - 1) + awaiting_redeploy, recorded_deaths)
        recorded_survival = sum(measurement(life, "SurvivedSeconds") for life in lives)
        totals["played_seconds"] += max(elapsed, recorded_survival)
        totals["recorded_survival"] += recorded_survival

        for life in lives:
            for field in ("Kills", "Shots", "Hits", "Score", "KillScore", "SurvivalScore"):
                totals[field] += count(life, field)
            if life.get("Version") == 2:
                legacy_hits = count(life, "LegacyHits")
                totals["legacy_shots"] += count(life, "LegacyShots")
                totals["legacy_hits"] += legacy_hits
                totals["verified_hits"] += max(0, count(life, "Hits") - legacy_hits)
                totals["verified_misses"] += count(life, "Misses")
                totals["pending_rounds"] += count(life, "PendingRounds")
                totals["unverified_rounds"] += count(life, "UnverifiedRounds")
            else:
                totals["legacy_shots"] += count(life, "Shots")
                totals["legacy_hits"] += count(life, "Hits")
            totals["best_score"] = max(totals["best_score"], count(life, "Score"))
            totals["longest_survival"] = max(totals["longest_survival"], measurement(life, "SurvivedSeconds"))
            totals["partial"] += life.get("Partial") is True
            outcome = life.get("EndReason")
            if isinstance(outcome, str):
                deployment_outcomes[outcome] += 1
            killer = life.get("Killer")
            if outcome in ("Died", "KIA") and isinstance(killer, str) and killer:
                deaths_by_operator[killer] += 1
            entries = life.get("Encounters")
            if isinstance(entries, list):
                for entry in entries:
                    if isinstance(entry, dict):
                        add_encounter(entry, totals, encounter_outcomes, kills_by_operator)

    if lifetime_seconds is not None:
        totals["played_seconds"] = lifetime_seconds
    return sections(totals, deployment_outcomes, encounter_outcomes, deaths_by_operator, kills_by_operator)


def played_seconds(state, lives):
    return max(measurement(state, "SimulationSeconds"),
               sum(measurement(life, "SurvivedSeconds") for life in lives))


def add_encounter(entry, totals, outcomes, kills_by_operator):
    totals["encounters"] += 1
    tracked = entry.get("Tracked") is True
    committed = tracked and entry.get("Committed") is True
    contact = entry.get("ReachedContact") is True
    matched_round = entry.get("MatchedRound") is True
    kill = count(entry, "KillPoints") > 0
    totals["tracked"] += tracked
    totals["committed"] += committed
    totals["contacts"] += contact
    totals["tracked_contacts"] += tracked and contact
    totals["converted"] += committed and kill
    totals["matched_encounters"] += matched_round
    totals["counter_encounters"] += entry.get("CounterHunt") is True
    outcome = entry.get("Outcome")
    if isinstance(outcome, str):
        outcomes[outcome] += 1
    for field in ("LockSeconds", "FinalApproachSeconds", "GroundCovered", "UsefulClosure", "ReturnDamage"):
        totals[field] += measurement(entry, field)
    totals["Pulses"] += count(entry, "Pulses")
    if tracked and contact:
        totals["TimeToContact"] += measurement(entry, "TimeToContact")
        rounds = count(entry, "RoundsToFirstHit")
        if rounds:
            totals["first_hit_samples"] += 1
            totals["RoundsToFirstHit"] += rounds
    for field in ("KillPoints", "PursuitPoints", "EngagementPoints", "CounterPoints", "PrecisionPoints", "RangePoints"):
        totals[field] += count(entry, field)
    if not kill:
        return
    totals["wound_kills" if entry.get("WoundKill") is True else "direct_kills"] += 1
    totals["counter_kills"] += entry.get("CounterHunt") is True
    totals["matched_kills"] += matched_round
    operator = entry.get("Callsign") or entry.get("OperatorId")
    if isinstance(operator, str) and operator:
        kills_by_operator[operator] += 1
    distance = measurement(entry, "CreditedRange")
    if distance:
        totals["ranged_kills"] += 1
        totals["CreditedRange"] += distance
        totals["longest_kill"] = max(totals["longest_kill"], distance)
    rounds = count(entry, "PrecisionRounds")
    if matched_round and rounds:
        totals["precision_samples"] += 1
        # These are per-kill snapshots of cumulative deployment accuracy.
        totals["precision_ratio_sum"] += min(1, count(entry, "PrecisionHits") / rounds)


def sections(totals, deployments, encounters, deaths_by_operator, kills_by_operator):
    def ratio(numerator, denominator):
        return numerator / denominator if denominator else None

    recorded_deaths = deployments["KIA"] + deployments["Suicide"] + deployments["Died"]
    groups = [
        ("Deployments", [
            ("Saved Hunts", totals["hunts"]),
            ("Deployments", totals["deployments"]),
            ("Recorded Deployments", totals["recorded_deployments"]),
            ("Partially Recorded", totals["partial"]),
            ("Open Recorded Deployments", deployments[""]),
            ("Deaths", totals["deaths"]),
            ("Killed in Action", deployments["KIA"]),
            ("Suicides", deployments["Suicide"]),
            ("Other Recorded Deaths", deployments["Died"]),
            ("Deaths Without a Score Record", totals["deaths"] - recorded_deaths),
            ("Time Played", totals["played_seconds"], "duration"),
            ("Recorded Survival Time", totals["recorded_survival"], "duration"),
            ("Longest Recorded Survival", totals["longest_survival"], "duration"),
            ("Average Recorded Survival", ratio(totals["recorded_survival"], totals["recorded_deployments"]), "duration"),
        ]),
        ("Combat", [
            ("Operator Kills", totals["Kills"]),
            ("Direct Kills", totals["direct_kills"]),
            ("Wound Kills", totals["wound_kills"]),
            ("Counter-hunt Kills", totals["counter_kills"]),
            ("Kills With a Matched Round", totals["matched_kills"]),
            ("Shots Fired", totals["Shots"]),
            ("Verified Hits", totals["verified_hits"]),
            ("Verified Misses", totals["verified_misses"]),
            ("Pending Rounds", totals["pending_rounds"]),
            ("Unverified Rounds", totals["unverified_rounds"]),
            ("Accuracy", ratio(totals["verified_hits"], totals["verified_hits"] + totals["verified_misses"])
             if not (totals["pending_rounds"] or totals["unverified_rounds"]) else None, "percent"),
            ("Earlier Accuracy Estimate", ratio(totals["legacy_hits"], totals["legacy_shots"]), "percent"),
            ("Encounters With a Matched Round", totals["matched_encounters"]),
            ("Counter-hunt Encounters", totals["counter_encounters"]),
            ("Return Damage", totals["ReturnDamage"], "decimal"),
            ("Kills With Credited Range", totals["ranged_kills"]),
            ("Combined Credited Kill Range", totals["CreditedRange"], "distance"),
            ("Longest Credited Kill", totals["longest_kill"] if totals["ranged_kills"] else None, "distance"),
            ("Average Credited Kill Range", ratio(totals["CreditedRange"], totals["ranged_kills"]), "distance"),
            ("Kills With a Precision Sample", totals["precision_samples"]),
            ("Average Scored Precision", ratio(totals["precision_ratio_sum"], totals["precision_samples"]), "percent"),
        ]),
        ("Pursuit", [
            ("Recorded Encounters", totals["encounters"]),
            ("Encounters Tracked", totals["tracked"]),
            ("Committed Pursuits", totals["committed"]),
            ("Contacts Reached", totals["contacts"]),
            ("Contacts Reached While Tracking", totals["tracked_contacts"]),
            ("Pursuits Converted", totals["converted"]),
            ("Pursuit Conversion Rate", ratio(totals["converted"], totals["committed"]), "percent"),
            ("Open Encounters", encounters[""]),
            ("Encounters Ending in Signal Loss", encounters["Signal lost"]),
            ("Encounters Ending With Target Eliminated", encounters["Target eliminated"]),
            ("Encounters Ending With Deployment", encounters["Life ended"]),
            ("Pulses Received", totals["Pulses"]),
            ("Active Pursuit Time", totals["LockSeconds"], "duration"),
            ("Final Approach Time", totals["FinalApproachSeconds"], "duration"),
            ("Total Time to Contact", totals["TimeToContact"], "duration"),
            ("Average Time to Contact", ratio(totals["TimeToContact"], totals["tracked_contacts"]), "duration"),
            ("Ground Covered", totals["GroundCovered"], "distance"),
            ("Useful Closure", totals["UsefulClosure"], "distance"),
            ("First Hits With a Round Match", totals["first_hit_samples"]),
            ("Rounds to Recorded First Hits", totals["RoundsToFirstHit"]),
            ("Average Rounds to First Hit", ratio(totals["RoundsToFirstHit"], totals["first_hit_samples"]), "decimal"),
        ]),
        ("Points", [
            ("Total Score", totals["Score"]),
            ("Best Score", totals["best_score"]),
            ("Average Deployment Score", ratio(totals["Score"], totals["recorded_deployments"]), "decimal"),
            ("Kill Score", totals["KillScore"]),
            ("Survival Points", totals["SurvivalScore"]),
            ("Base Kill Points", totals["KillPoints"]),
            ("Pursuit Points", totals["PursuitPoints"]),
            ("Engagement Points", totals["EngagementPoints"]),
            ("Counter-hunt Points", totals["CounterPoints"]),
            ("Precision Points", totals["PrecisionPoints"]),
            ("Range Points", totals["RangePoints"]),
        ]),
    ]
    for title, counts in (("Operators Eliminated", kills_by_operator), ("Deaths by Operator", deaths_by_operator)):
        if counts:
            groups.append((title, sorted(counts.items(), key=lambda entry: (-entry[1], entry[0].casefold()))))

    return [
        {"title": title, "rows": [
            {"label": label, "value": value, "format": kind[0] if kind else "count"}
            for label, value, *kind in rows
        ]}
        for title, rows in groups
    ]
