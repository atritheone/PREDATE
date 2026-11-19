from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import io
import json
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import process_guard
import world


class GuardTests(unittest.TestCase):
    def test_mission_compile_error_closes_owned_process_promptly(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
            identity = world.process_identity(child.pid)
            self.assertIsNotNone(identity)
            result = []
            def watch(): result.append(process_guard.watch(identity, folder, folder / "failure.json", poll=.02))
            worker = threading.Thread(target=watch)
            worker.start()
            try:
                started = time.time()
                (folder / "script.log").write_text("Can't find variable 'GuardFixture'\nCan't compile mission init script'!\n")
                worker.join(timeout=3)
                self.assertFalse(worker.is_alive(), "Failure was not handled promptly")
                child.wait(timeout=1)
                self.assertEqual(result[0]["kind"], "compile")
                self.assertIn("GuardFixture", result[0]["message"])
                self.assertLess(result[0]["closed_at"] - started, 0.7)
            finally:
                if child.poll() is None: child.terminate()
                child.wait()
                worker.join(timeout=1)

    def test_old_failures_and_regular_warnings_are_ignored(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            log = folder / "script.log"
            log.write_text("SCRIPT (W): Missing script declaration for method\n")
            self.assertIsNone(process_guard.detect_failure(folder, time.time() - 1, {}))
            log.write_text("Can't compile mission init script'!\n")
            self.assertIsNone(process_guard.detect_failure(folder, time.time() + 10, {}))

    def test_native_dump_is_caught_without_script_log(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder / "ErrorMessage_DayZ.mdmp").write_bytes(b"MDMP")
            failure = process_guard.detect_failure(folder, time.time() - 1, {})
            self.assertEqual(failure["kind"], "native")

    def test_intentional_pause_ignores_only_matching_hang_snapshots(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder = root / 'profile'; folder.mkdir()
            identity = {'pid': 17, 'created': 18, 'executable': 'owned.exe'}
            (root / 'intentional-pause.json').write_text(json.dumps({
                'server': identity, 'paused_at': time.time() - 1, 'resumed_at': time.time()}))
            dump = folder / 'FROZEN_DayZServer.mdmp'; dump.write_bytes(b'MDMP')
            offsets = {}
            self.assertIsNone(process_guard.detect_failure(folder, time.time() - 2, offsets, identity))
            # Reused PID and unrelated exceptions retain immediate detection.
            reused = identity | {'created': 999}
            self.assertEqual(process_guard.detect_failure(folder, time.time() - 2, {}, reused)['kind'], 'native')
            (folder / 'ErrorMessage_DayZ.mdmp').write_bytes(b'MDMP')
            self.assertEqual(process_guard.detect_failure(folder, time.time() - 2, offsets, identity)['kind'], 'native')

    def test_hang_after_resume_grace_is_still_a_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); folder = root / 'client'; folder.mkdir()
            identity = {'pid': 17, 'created': 18, 'executable': 'owned.exe'}
            (root / 'intentional-pause.json').write_text(json.dumps({
                'client': identity, 'paused_at': time.time() - 40, 'resumed_at': time.time() - 30}))
            (folder / 'FROZEN_DayZ.mdmp').write_bytes(b'MDMP')
            self.assertEqual(process_guard.detect_failure(folder, time.time() - 2, {}, identity)['kind'], 'native')

    def test_compile_message_split_between_log_writes_is_caught(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            log = folder / "script.log"
            offsets = {}
            started = time.time() - 1
            log.write_text("Can't com")
            self.assertIsNone(process_guard.detect_failure(folder, started, offsets))
            with log.open("a") as stream:
                stream.write("pile mission init script'!\n")
            self.assertEqual(process_guard.detect_failure(folder, started, offsets)["kind"], "compile")

    def test_compile_failure_reports_primary_script_error_instead_of_cascade(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder / "DayZServer.RPT").write_text(
                "Can't find variable 'CascadingError'\nCan't compile Mission script module!\n")
            source = folder / "script_2026-11-18.log"
            source.write_text(
                'SCRIPT    (E): @"Predate/scripts/5_Mission/fixture.c,12": Undefined function \'RootCause\'\n'
                'SCRIPT    (E): @"scripts/5_Mission/secondary.c,31": Can\'t find variable \'CascadingError\'\n')
            failure = process_guard.detect_failure(folder, time.time() - 1, {})
            self.assertEqual(failure["kind"], "compile")
            self.assertIn("RootCause", failure["message"])
            self.assertEqual(Path(failure["log"]), source)

    def test_log_directory_can_appear_after_guard_starts(self):
        with tempfile.TemporaryDirectory() as temporary:
            self.assertIsNone(process_guard.detect_failure(Path(temporary) / "not-created-yet", time.time(), {}))

    def test_locked_diagnostic_file_does_not_prevent_closing_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
            try:
                identity = world.process_identity(child.pid)
                (folder / "script.log").write_text("Can't compile mission init script'!\n")
                with patch.object(process_guard, "atomic_json", side_effect=PermissionError("locked")), \
                        patch.object(process_guard.sys, "stderr", new=io.StringIO()):
                    failure = process_guard.watch(identity, folder, folder / "failure.json")
                child.wait(timeout=1)
                self.assertIn("closed_at", failure)
            finally:
                if child.poll() is None: child.terminate()
                child.wait()

    def test_reused_pid_is_never_terminated(self):
        identity = {"pid": 99999, "created": 1, "executable": "expected.exe"}
        with tempfile.TemporaryDirectory() as temporary, patch.object(world, "process_identity", return_value={
                "pid": 99999, "created": 2, "executable": "expected.exe"}), patch.object(process_guard.os, "kill") as kill:
            self.assertIsNone(process_guard.watch(identity, temporary, Path(temporary) / "failure.json"))
            kill.assert_not_called()


if __name__ == "__main__": unittest.main()
