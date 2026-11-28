"""Check player shot accounting and Expansion damage coordinates in a disposable operation."""

import argparse
import json
import os
from pathlib import Path
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

import build
import loadouts
import mod_presets
import session
import session_store as store
import world


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--operators", type=int, default=100)
    parser.add_argument("--buca", action="store_true")
    args = parser.parse_args()
    build.require_closed_runtime()
    os.environ["PREDATE_CANDIDATE_MOD"] = "1"
    build.pack(candidate=True)

    profile = f"validation-shot-score-{uuid.uuid4().hex[:6]}"
    extra_mods = [str(loadouts.WORKSHOP / "3092251597")] if args.buca else []
    operation = store.create_operation("Shot score regression", profile=profile, extra_mods=extra_mods,
                                       opponent_count=args.operators)
    operation["kind"] = "validation"
    store.atomic_json(world.WORLD / "operation.json", operation)
    world.MODS.extend(Path(value) for value in operation["extra_mods"])
    mod_presets.install_keys(operation["mod_lock"], world.SERVER / "keys")
    entry = world.MISSION / "init.c"
    fixture = (Path(__file__).parent / "shot_score_regression.c").read_text(encoding="utf-8")
    marker = "Mission CreateCustomMission(string path)"
    contents = entry.read_text(encoding="utf-8").replace(marker, fixture + "\n" + marker)
    entry.write_text(contents.replace("return new MissionServer;", "return new PredateShotScoreRegression;"),
                     encoding="utf-8")
    print(f"Validation profile: {profile}", flush=True)

    nonce = uuid.uuid4().hex
    client_process = None
    try:
        def open_client():
            nonlocal client_process
            client_process = world.launch_client(nonce, extra_args=["-window", "-x=1280", "-y=720"], wait_for_server=True)

        world.start(nonce, on_started=open_client)
        store.atomic_json(world.WORLD / "client" / "PredateClientCommand.json", {
            "Schema": 1, "SessionId": nonce, "Sequence": 1, "Action": "server-ready",
            "MapId": world.TERRAIN.id, "Port": world.PORT, "QueryPort": world.QUERY_PORT,
            "Password": world.settings()["join_password"],
        })
        result = {}
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            assert world.running(), "Validation server exited."
            failure = session.read_native(world.WORLD / "client/PredateFailure.json")
            assert not failure, failure
            client = session.read_native(world.WORLD / "client/PredateClientStatus.json")
            assert not client.get("Error"), client["Error"]
            result = session.read_native(world.PROFILE / "ShotScoreRegression.json")
            if result:
                break
            time.sleep(0.25)
        print(json.dumps(result, indent=2), flush=True)
        assert result.get("Unverified") == result.get("Pending") == 0, result
        spread_round = int(args.operators > 1)
        expected_counts = (4 + 2 * spread_round, 2 + 2 * spread_round, 2)
        assert (result.get("Shots"), result.get("Hits"), result.get("Misses")) == expected_counts, result
        assert result.get("ExtraOperatorHits") == 2 * spread_round, result
        assert result.get("HitPointsBeforeKill") == 350 * spread_round, result
        assert result.get("HitPointsAfterFirstKill") == 300 * spread_round, result
        assert result.get("BonusAfterFirstKill") == 0 and result.get("KillsAfterFirstKill") == spread_round, result
        assert result.get("NonLethalHitPoints") == 100 + 150 * spread_round, result
        assert result.get("MultiHitBonusPoints") == 300 * spread_round, result
        assert result.get("MultiKillBonusPoints") == 500 * spread_round, result
        assert result.get("Kills") == 2 * spread_round, result
        saved = store.read_json(world.PROFILE / "PredateLifeScore-1.json")
        assert saved["ExtraOperatorHits"] == 2 * spread_round, saved
        assert saved["NonLethalHitPoints"] == result["NonLethalHitPoints"], saved
        assert saved["MultiHitBonusPoints"] == result["MultiHitBonusPoints"], saved
        assert saved["MultiKillBonusPoints"] == result["MultiKillBonusPoints"], saved
        pursuit = next(entry for entry in saved["Encounters"] if entry["PursuitShots"])
        assert len([entry for entry in saved["Encounters"] if entry["PursuitShots"]]) == 1, saved
        assert (pursuit["PursuitShots"], pursuit["PursuitHits"], pursuit["PursuitPending"],
                pursuit["PursuitUnverified"]) == (3 + 2 * spread_round, 2 + spread_round, 0, 0), pursuit
        assert pursuit["PursuitExtraHits"] == 2 * spread_round, pursuit
        assert sum(entry["KillPoints"] for entry in saved["Encounters"]) == 2000 * spread_round, saved
        assert saved["Score"] == sum(saved[name] for name in (
            "KillScore", "SurvivalScore", "NonLethalHitPoints", "MultiHitBonusPoints", "MultiKillBonusPoints")), saved
        assert result.get("ExpiryReleased") and result.get("QuitReleased") and result.get("ResumedMisses") == 1, result
        assert result.get("RecoveredPursuit"), result
        assert result.get("LaterDeploymentHits") == 0 and result.get("BuckshotLifetime", 0) > 0, result
        if args.buca:
            assert result.get("BucaLifetime", 0) > 0, result
        assert result["TargetHealth"] < 100, result
        assert abs(result["CreditedRange"] - result["ExpectedRange"]) < 2, result
    finally:
        try:
            if client_process and client_process.poll() is None:
                client_path = world.WORLD / "client"
                status = session.read_native(client_path / "PredateClientStatus.json")
                store.atomic_json(client_path / "PredateClientCommand.json", {
                    "Schema": 1, "SessionId": nonce, "Sequence": status.get("Acknowledged", 1) + 1,
                    "Action": "server-closed",
                })
                client_process.wait(timeout=30)
        finally:
            if world.running():
                world.stop(make_backup=False)


if __name__ == "__main__":
    main()
