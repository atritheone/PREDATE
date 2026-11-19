"""Fast failure guard for an explicitly owned game/server process."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import signal
import sys
import time
import platform_win

import world
from session_store import atomic_json

FATAL = re.compile(r"can't compile\b|virtual machine exception|unhandled exception|fault address:", re.I)
COMPILER_DETAIL = re.compile(r"Can't find variable|Undefined function|Syntax error|Error.*line", re.I)
SCRIPT_COMPILER_ERROR = re.compile(r"\bSCRIPT\s*\(E\):.*\.c[,(]\d+", re.I)


def redact(text):
    return re.sub(r"(?i)(\b(?:rconpassword|password)\s*(?:=|\s)\s*)[^\s;\"]+", r"\1<redacted>", text)


def alive(identity):
    return world.process_identity(identity["pid"]) == identity


def record_failure(path, failure):
    try:
        atomic_json(path, failure)
    except OSError as error:
        # A locked diagnostic file must not leave a failed process and popup open.
        print(f"Failure record could not be written: {error}", file=sys.stderr, flush=True)


def expected_pause_dump(path, identity):
    """A hang snapshot during our deliberate suspension is not a crash."""
    if not identity or not path.name.startswith('FROZEN_'): return False
    try:
        marker = json.loads((path.parent.parent / 'intentional-pause.json').read_text())
        matched = any(all(marker.get(role, {}).get(key) == value for key, value in identity.items())
                      for role in ('server', 'client'))
        modified = path.stat().st_mtime
        resumed = marker.get('resumed_at')
        return matched and modified >= marker['paused_at'] - 1 and (resumed is None or modified <= resumed + 15)
    except (OSError, ValueError, KeyError, TypeError): return False


def primary_compiler_error(logdir, started):
    for path in sorted(Path(logdir).glob("script*.log"), reverse=True):
        try:
            if path.stat().st_mtime < started - 1: continue
            with path.open("rb") as stream:
                startup = stream.read(512 * 1024).decode("utf-8", errors="replace")
        except OSError:
            continue
        for line in startup.splitlines():
            if SCRIPT_COMPILER_ERROR.search(line):
                return line.strip(), path
    return None


def detect_failure(logdir, started, offsets, identity=None):
    try:
        paths = list(Path(logdir).iterdir())
    except OSError:
        return None
    for path in paths:
        if path.suffix.lower() == ".mdmp":
            try:
                stamp = path.stat().st_mtime_ns
                if offsets.get(path) == ('expected-pause', stamp): continue
                if expected_pause_dump(path, identity):
                    offsets[path] = ('expected-pause', stamp)
                    continue
                if path.stat().st_mtime >= started - 1:
                    return {"kind": "native", "message": "DayZ created an error dump: " + path.name,
                            "log": str(path), "excerpt": "Native error dump created before a usable error log"}
            except OSError: pass
            continue
        if path.suffix.lower() not in {".log", ".rpt"}: continue
        if path.name.startswith("guard-"): continue
        try:
            stat = path.stat()
            if stat.st_mtime < started - 1: continue
            previous, pending = offsets.get(path, (0, ""))
            if stat.st_size == previous: continue
            if stat.st_size < previous: previous, pending = 0, ""
            with path.open("rb") as stream:
                stream.seek(previous)
                chunk = pending + stream.read().decode("utf-8", errors="replace")
                position = stream.tell()
            # Log writes can split the fatal phrase between two polls.
            offsets[path] = (position, chunk[-2048:])
        except (OSError, ValueError): continue
        if FATAL.search(chunk):
            lines = chunk.splitlines()
            fatal = next(line.strip() for line in lines if FATAL.search(line))
            compile_failure = "can't compile" in chunk.lower()
            detail = next((line.strip() for line in lines if COMPILER_DETAIL.search(line)), None)
            primary = primary_compiler_error(logdir, started) if compile_failure else None
            diagnostic = path
            if primary:
                detail, diagnostic = primary
            excerpt = chunk[-6000:]
            if detail and detail not in excerpt: excerpt = detail + "\n" + excerpt
            return {"kind": "compile" if compile_failure else "runtime",
                    "message": redact(detail or fatal), "log": str(diagnostic), "excerpt": redact(excerpt)}
    return None


def watch(identity, logdir, failure_file, server_state=None, stop_file=None, poll=0.1):
    # FILETIME is UTC in 100ns units since 1601. Never inspect earlier runs' errors.
    started = identity["created"] / 10_000_000 - 11_644_473_600
    offsets = {}
    while True:
        if stop_file and Path(stop_file).exists(): return None
        failure = detect_failure(logdir, started, offsets, identity)
        if failure:
            failure.update(schema=1, process=identity, detected_at=time.time())
            record_failure(failure_file, failure)
            # A compile failure has not loaded a world. A native unhandled exception
            # cannot safely continue. Close both immediately, including their popup.
            # A script runtime failure gets a brief opportunity for managed clean stop.
            if failure["kind"] == "runtime" and "virtual machine exception" in failure["excerpt"].lower() and server_state:
                try:
                    state = json.loads(Path(server_state).read_text())
                    if state.get("session_id"):
                        control = Path(logdir) / "PredateControl.json"
                        atomic_json(control, {"Schema": 1, "SessionId": state["session_id"],
                                              "Sequence": 2_000_000_000, "Action": "stop"})
                        deadline = time.monotonic() + 1
                        while alive(identity) and time.monotonic() < deadline: time.sleep(poll)
                except (OSError, ValueError): pass
            if alive(identity):
                try:
                    os.kill(identity["pid"], signal.SIGTERM)
                except OSError:
                    if alive(identity): raise
            deadline = time.monotonic() + 3
            while alive(identity) and time.monotonic() < deadline: time.sleep(0.05)
            if not alive(identity): failure["closed_at"] = time.time()
            else: failure["close_error"] = "Failed process did not exit within three seconds"
            record_failure(failure_file, failure)
            return failure
        if not alive(identity): return None
        time.sleep(poll)


def discover_client(executable, since, logdir, failure):
    deadline = time.monotonic() + 150
    while time.monotonic() < deadline:
        for pid in platform_win.find_processes("DayZ_x64.exe"):
            identity = world.process_identity(pid)
            if not identity: continue
            created = identity["created"] / 10_000_000 - 11_644_473_600
            if created < since - 0.1: continue
            if identity["executable"].casefold() != str(executable).casefold(): continue
            return watch(identity, logdir, failure)
        time.sleep(0.1)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--created", type=int)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--logdir", type=Path, required=True)
    parser.add_argument("--failure", type=Path, required=True)
    parser.add_argument("--server-state", type=Path)
    parser.add_argument("--stop-file", type=Path)
    parser.add_argument("--discover-client", action="store_true")
    parser.add_argument("--since", type=float)
    args = parser.parse_args()
    if args.discover_client:
        discover_client(args.exe, args.since, args.logdir, args.failure)
        return
    if args.pid is None or args.created is None:
        parser.error("--pid and --created are required for an owned process")
    watch({"pid": args.pid, "created": args.created, "executable": args.exe},
          args.logdir, args.failure, args.server_state, args.stop_file)


if __name__ == "__main__": main()
