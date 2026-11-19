"""Real-engine session checks in a disposable clone using the packaged backend.

No source world is opened or changed. Fixtures set non-default health, blood and
ammunition only inside the new validation world. Run with DayZ/server closed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import platform_win
import session
import session_store as store
import world


def native_evidence():
    """Identify actual packaged inputs, rather than a potentially dirty Git head."""
    paths = {
        "addon_sha256": world.MODS[-1] / "addons/Predate.pbo",
        "mission_sha256": world.MISSION / "init.c",
        "dependency_lock_sha256": ROOT / "config/release-lock.json",
    }
    return {key: hashlib.sha256(path.read_bytes()).hexdigest() for key, path in paths.items()}


class Bridge:
    def __init__(self, source=False, candidate=False, additional_env=None, bundled=None):
        bundled = Path(bundled) if bundled else ROOT / "launcher-app/backend/PREDATEBackend.exe"
        sha = hashlib.sha256(bundled.read_bytes()).hexdigest()[:16]
        executable = ROOT / "session-runtime/backend" / sha / "PREDATEBackend.exe"
        executable.parent.mkdir(parents=True, exist_ok=True)
        if not executable.exists(): shutil.copy2(bundled, executable)
        self.log = (ROOT / "build/engine-session-bridge.log").open("ab")
        arguments = [sys.executable, str(ROOT/'scripts/backend_entry.py'), 'bridge'] if source else [str(executable), 'bridge']
        environment = dict(os.environ, PREDATE_ROOT=str(ROOT))
        if additional_env: environment.update(additional_env)
        if candidate: environment['PREDATE_CANDIDATE_MOD']='1'
        self.process = subprocess.Popen(arguments, cwd=ROOT,
            env=environment, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=self.log, text=True,
            creationflags=subprocess.CREATE_NO_WINDOW)
        self.sequence = 0

    def call(self, method, params=None):
        self.sequence += 1
        self.process.stdin.write(json.dumps({"id": self.sequence, "method": method, "params": params or {}}) + "\n")
        self.process.stdin.flush()
        response = json.loads(self.process.stdout.readline())
        if not response["ok"]: raise RuntimeError(response["error"])
        return response["result"]

    def action(self, action, **values):
        return self.call("action", {"action": action, **values})["requestId"]

    def wait_action(self, identifier, timeout=240):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            result = self.call("result", {"requestId": identifier})
            if result:
                if not result["ok"]: raise RuntimeError(result.get("error", "Session action failed"))
                return result
            time.sleep(.2)
        raise TimeoutError("Session action timed out")

    def wait_phase(self, phases, timeout=180):
        deadline = time.monotonic() + timeout
        notice = 0
        while time.monotonic() < deadline:
            status = self.call("status")
            if status["phase"] in phases: return status
            if status["phase"] == "error": raise RuntimeError(status["message"])
            if time.monotonic() - notice > 20:
                print("PHASE:", status["phase"], flush=True); notice = time.monotonic()
            time.sleep(.25)
        raise TimeoutError("Expected phase: " + ", ".join(phases))

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=10)
        self.log.close()


FIXTURE = '''
// Test fixture: this subclass exists only in a disposable validation mission.
class PredateValidationMission : MissionServer
{
    bool m_ForceSessionEquipment;
    float m_SessionEquipmentReadyAt;
    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PredateSeedBot, 7500, false);
    }

    void PredateSeedBot()
    {
        if (FileExist("$profile:ValidationBotSeeded.txt")) return;
        eAIBase actor = m_PredateSession.m_HuntBot;
        if (!actor) return;
        Weapon_Base weapon = Weapon_Base.Cast(actor.GetHumanInventory().GetEntityInHands());
        if (!weapon) return;
        Magazine magazine = weapon.GetMagazine(weapon.GetCurrentMuzzle());
        if (!magazine) return;
        actor.SetHealth("", "Health", 73);
        actor.SetHealth("", "Blood", 4321);
        magazine.ServerSetAmmoCount(17);
        m_PredateSession.PredateSave();
        FileHandle marker = OpenFile("$profile:ValidationBotSeeded.txt", FileMode.WRITE);
        FPrintln(marker, "fixture seeded once");
        CloseFile(marker);
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        // The fixed-ammunition session fixture measures persistence, not a
        // firefight with whichever separation its source save happens to have.
        if (m_ForceSessionEquipment && m_PredateSession.m_HuntBot)
            m_PredateSession.m_HuntBot.DisableSimulation(true);
        if (FileExist("$profile:ValidationPlayerSeeded.txt")) return;
        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);
        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);
            if (!player || !player.GetIdentity() || eAIBase.Cast(player)) continue;
            Weapon_Base weapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
            Magazine magazine;
            if (weapon) magazine = weapon.GetMagazine(weapon.GetCurrentMuzzle());
            if ((!weapon || !magazine) && m_ForceSessionEquipment)
            {
                if (!m_PredateSession.m_ActorsActive || player.GetDayZPlayerInventory().IsProcessing()) continue;
                if (GetGame().GetTickTime() < m_SessionEquipmentReadyAt) continue;
                EntityAI held = player.GetHumanInventory().GetEntityInHands();
                if (held)
                {
                    player.GetInventory().DropEntity(InventoryMode.SERVER, player, held);
                    m_SessionEquipmentReadyAt = GetGame().GetTickTime()+1;
                    continue;
                }
                // Existing restored inventory is retained. Never call
                // RemoveAllItems while repairing a test's hands precondition.
                Weapon_Base fixtureRifle = Weapon_Base.Cast(player.GetHumanInventory().CreateInHands("M4A1"));
                if (fixtureRifle)
                {
                    fixtureRifle.GetInventory().CreateAttachment("M4_RISHndgrd");
                    fixtureRifle.GetInventory().CreateAttachment("M4_CQBBttstck");
                    fixtureRifle.GetInventory().CreateAttachment("ACOGOptic");
                    fixtureRifle.SpawnAttachedMagazine("Mag_STANAG_30Rnd");
                }
                weapon = Weapon_Base.Cast(player.GetHumanInventory().GetEntityInHands());
                if (weapon) magazine = weapon.GetMagazine(weapon.GetCurrentMuzzle());
            }
            if (!weapon || !magazine) continue;
            player.SetHealth("", "Health", 81);
            player.SetHealth("", "Blood", 4555);
            magazine.ServerSetAmmoCount(13);
            player.Save();
            FileHandle marker = OpenFile("$profile:ValidationPlayerSeeded.txt", FileMode.WRITE);
            FPrintln(marker, "fixture seeded once");
            CloseFile(marker);
        }
    }
}
'''


def fixture(source, session_equipment=False):
    source_base = store.world_path(source)
    progress = json.loads((source_base / "profile/PredateProgress.json").read_text())
    if progress.get("Defeated"): raise ValueError("Select a source with a living opponent")
    profile = "validation-b-" + uuid.uuid4().hex[:6]
    world.select_profile(profile)
    world.setup()
    for original, relative in store.save_files(source):
        target = world.WORLD / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
    entry = world.MISSION / "init.c"
    text = entry.read_text()
    marker = "Mission CreateCustomMission(string path)"
    assert text.count(marker) == 1
    text = text.replace(marker, FIXTURE + "\n" + marker).replace("return new MissionServer;", "return new PredateValidationMission;")
    if session_equipment:
        text = text.replace('return new PredateValidationMission;',
            'PredateValidationMission validationMission = new PredateValidationMission;\n'
            '    validationMission.m_ForceSessionEquipment = true;\n'
            '    return validationMission;')
    entry.write_text(text, encoding="utf-8")
    store.atomic_json(world.WORLD / "operation.json", {"schema": 1, "id": profile,
        "name": "Session validation", "kind": "validation", "created": None, "extra_mods": []})
    return profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", default="main")
    parser.add_argument("--require-memory", action="store_true")
    args = parser.parse_args()
    if platform_win.find_processes("DayZ_x64.exe") or platform_win.find_processes("DayZServer_x64.exe"):
        raise RuntimeError("Existing game/server must be closed")
    original_preferences = store.preferences()
    profile = fixture(args.source, session_equipment=True)
    bridge = Bridge()
    report = {"profile": profile, "passed": False, "packaged_backend": True}
    report['explicit_test_equipment_when_needed'] = True
    report['test_ai_simulation_held'] = True
    report.update(native_evidence())
    report['backend_sha256'] = store.digest(ROOT/'launcher-app/backend/PREDATEBackend.exe')
    try:
        identifier = bridge.action("continue", profile=profile)
        bridge.wait_action(identifier)
        owner = session.read_native(session.MANAGER)
        report['supervisor_executable'] = owner['executable']
        report['supervisor_sha256'] = store.digest(Path(owner['executable']))
        assert report['supervisor_sha256'] == report['backend_sha256'], 'This packaged check requires the matching frozen supervisor'
        bridge.wait_phase({"playing"})
        print("CONNECTED:", profile, flush=True)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            status = world.native_status()
            if status.get("BotMagazine") == 17 and status.get("PlayerMagazine") == 13: break
            time.sleep(.25)
        assert status["BotMagazine"] == 17, status
        assert status["PlayerMagazine"] == 13, status
        assert 70 < status["BotHealth"] < 80, status
        assert 78 < status["PlayerHealth"] < 90, status
        scripts = "\n".join(p.read_text(errors="replace") for p in world.PROFILE.glob("script*.log"))
        memory = "Restored private contact memory without sampling" in scripts
        report["contact_memory_restored"] = memory
        if args.require_memory: assert memory, "Saved AI contact memory was not restored"
        report["nondefault_combat_state"] = True
        bridge.wait_action(bridge.action("quit"))
        bridge.wait_phase({"suspended"})
        assert not world.running()
        assert not platform_win.find_processes("DayZ_x64.exe")
        report["normal_save_quit"] = True
        progress = json.loads((world.PROFILE / "PredateProgress.json").read_text())
        saved = json.loads((world.PROFILE / "PredateWorldState.json").read_text())
        before = {relative: store.digest(path) for path, relative in store.save_files(profile)}
        time.sleep(2)
        assert before == {relative: store.digest(path) for path, relative in store.save_files(profile)}
        report["no_progress_while_closed"] = True
        identifier = bridge.action("continue", profile=profile)
        bridge.wait_action(identifier)
        ready = world.native_status()
        assert abs(ready["SimulationSeconds"] - saved["SimulationSeconds"]) < .02, ready
        assert abs(ready["PulseRemaining"] - progress["PulseRemaining"]) < .02, ready
        assert ready["BotMagazine"] == 17, ready
        assert 70 < ready["BotHealth"] < 80, ready
        assert saved.get("HasBotVitals"), saved
        assert abs(ready["BotBlood"] - saved["BotBlood"]) < .1, ready
        assert abs(ready["BotShock"] - saved["BotShock"]) < .1, ready
        report["blood_and_shock_restored"] = True
        bridge.wait_phase({"playing"})
        time.sleep(2)
        resumed = world.native_status()
        assert resumed["PlayerMagazine"] == 13, resumed
        assert 78 < resumed["PlayerHealth"] < 90, resumed
        report["ammunition_injuries_and_timers_restored"] = True
        bridge.wait_action(bridge.action("quit"))
        bridge.wait_phase({"suspended"})
        report["passed"] = True
    finally:
        try:
            if world.running() or platform_win.find_processes("DayZ_x64.exe"):
                bridge.wait_action(bridge.action("quit"), timeout=90)
        finally:
            try:
                bridge.close()
                report["bridge_closed_independently"] = True
            finally:
                store.save_preferences({key: value for key, value in original_preferences.items() if key != "schema"})
                store.atomic_json(ROOT / "build/engine-session-results.json", report)
                print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__": main()
