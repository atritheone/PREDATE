"""Check player shot accounting and Expansion damage coordinates in a disposable operation."""

import argparse
import json
import os
from pathlib import Path
import sys
import time
import uuid

from run_elite_acquisition import close_client

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
    try:
        def open_client():
            world.launch_client(nonce, extra_args=["-window", "-x=1920", "-y=1080"], wait_for_server=True)

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
        assert (result.get("Shots"), result.get("Hits"), result.get("Misses")) == (4, 2, 2), result
        assert result.get("ExpiryReleased") and result.get("QuitReleased") and result.get("ResumedMisses") == 1, result
        assert result.get("LaterDeploymentHits") == 0 and result.get("BuckshotLifetime", 0) > 0, result
        if args.buca:
            assert result.get("BucaLifetime", 0) > 0, result
        assert result["TargetHealth"] < 100, result
        assert abs(result["CreditedRange"] - result["ExpectedRange"]) < 2, result
    finally:
        try:
            if nonce:
                close_client(nonce)
        finally:
            if world.running():
                world.stop(make_backup=False)


if __name__ == "__main__":
    main()
