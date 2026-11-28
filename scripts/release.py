"""Release inputs and fingerprints; no downloads, saves or credentials are managed here."""
from pathlib import Path
import ctypes
import hashlib
import json
import os
import re
import subprocess

from app_runtime import ROOT
import maps
STEAM = Path(r"C:\Program Files (x86)\Steam\steamapps")
WORKSHOP = STEAM / "workshop/content/221100"
LOCK = ROOT / "config/release-lock.json"
REALISM_MODS = {"@BetterSnapSounds": "2565724570", "@ReactiveRagdolls": "3824364587"}
DEPENDENCIES = {"@CF": "1559212036", "@DabsFramework": "2545327648",
                "@ExpansionCore": "2291785308", "@ExpansionAI": "2792982069",
                **REALISM_MODS}


def installed_realism_mod(value):
    path = Path(value).resolve()
    workshop_path = tuple(part.casefold() for part in path.parts[-5:-1])
    for name, item in REALISM_MODS.items():
        staged = ROOT / "mods" / name
        if path == staged or (workshop_path == ("steamapps", "workshop", "content", "221100") and path.name == item):
            return staged
    metadata = path / "meta.cpp"
    if metadata.is_file():
        match = re.search(r"(?m)^\s*publishedid\s*=\s*(\d+)\s*;", metadata.read_text(errors="replace"))
        if match:
            return next((ROOT / "mods" / name for name, item in REALISM_MODS.items() if item == match[1]), None)
    return None


def snapshot_matches(saved, current, map_id="chernarusplus"):
    if saved == current:
        return True
    if not isinstance(saved, dict) or not isinstance(saved.get("dependencies"), dict) or not current:
        return False
    dependencies = saved["dependencies"]
    added = {name: current["dependencies"][name] for name in REALISM_MODS
             if name in current["dependencies"] and name not in dependencies}
    saved, current = dict(saved), dict(current)
    for entry in (saved, current):
        for item in maps.TERRAINS.values():
            if item.id != map_id: entry.pop(item.mission_lock, None)
        packages = entry.pop("terrain_packages", {})
        if map_id in packages:
            entry["terrain_packages"] = {map_id: packages[map_id]}
    return {**saved, "dependencies": dependencies | added} == current


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


if os.name == "nt":
    import msvcrt
    from ctypes import wintypes

    class _FileBasicInfo(ctypes.Structure):
        _fields_ = [(name, ctypes.c_longlong) for name in
                    ("created", "accessed", "written", "changed")] + [("attributes", wintypes.DWORD)]

    _file_info = ctypes.WinDLL("kernel32", use_last_error=True).GetFileInformationByHandleEx
    _file_info.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    _file_info.restype = wintypes.BOOL


def _file_change_time(stream, stat):
    if os.name != "nt":
        return stat.st_ctime_ns
    # Python's st_ctime is creation time on Windows. The NTFS change time
    # also invalidates an in-place update that restores the old write time.
    info = _FileBasicInfo()
    if _file_info(msvcrt.get_osfhandle(stream.fileno()), 0, ctypes.byref(info), ctypes.sizeof(info)):
        return info.changed or None
    return None


def _file_stamp(stat):
    return [stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns,
            getattr(stat, "st_birthtime_ns", None)]


class LaunchFingerprints:
    """Cache launch-time SHA-256 results for an unchanged file identity."""

    def __init__(self, path):
        self.path = path
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            data = None
        self.cached = data["files"] if isinstance(data, dict) and data.get("schema") == 1 and isinstance(data.get("files"), dict) else {}
        self.checked = {}

    def fingerprint(self, path, expected=None):
        path = Path(path)
        key = os.path.normcase(str(path.resolve(strict=True)))
        before = path.stat()
        stamp = _file_stamp(before)
        with path.open("rb") as stream:
            change = _file_change_time(stream, before)
            identity = stamp + [change]
            entry = self.checked.get(key, self.cached.get(key))
            cacheable = before.st_ino != 0 and change is not None
            if (cacheable and isinstance(entry, list) and len(entry) == 2 and entry[0] == identity
                    and isinstance(entry[1], str) and re.fullmatch(r"[0-9a-f]{64}", entry[1])
                    and (expected is None or entry[1] == expected)):
                result = entry[1]
            else:
                result = hashlib.file_digest(stream, "sha256").hexdigest()
            after = path.stat()
            if _file_stamp(after) != stamp or _file_change_time(stream, after) != change:
                raise RuntimeError(f"File changed while checking it. Let its update finish, then retry: {path.name}")
        if cacheable and (expected is None or result == expected):
            self.checked[key] = [identity, result]
        return result

    def save(self):
        files = self.cached | self.checked
        if files == self.cached:
            return
        from session_store import atomic_json

        try:
            atomic_json(self.path, {"schema": 1, "files": files})
        except OSError:
            # Cache failure must never block a correctly verified launch.
            pass


def write_changed(path, data):
    path = Path(path)
    if isinstance(data, str): data = data.encode("utf-8")
    if not path.exists() or path.read_bytes() != data:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def fingerprints(folder, suffixes=None, hash_file=None):
    hash_file = hash_file or digest
    return {p.relative_to(folder).as_posix(): hash_file(p) for p in sorted(folder.rglob("*"))
            if p.is_file() and (suffixes is None or p.suffix.lower() in suffixes)}


def terrain_fingerprints(folder):
    return fingerprints(folder, {".pbo", ".ebo", ".bisign"})


def verify_terrain(terrain, client, server):
    from session_store import atomic_json, read_json

    expected = json.loads(LOCK.read_text()).get("terrain_packages", {}).get(terrain.id, {})
    cache_path = ROOT / "session-runtime" / f"verified-{terrain.id}.json"
    cached = read_json(cache_path, {})
    checked = {}
    for side, base in (("client", client), ("server", server)):
        if not expected.get(side):
            raise RuntimeError(f"{terrain.name} map verification data is missing.")
        for name, wanted in expected[side].items():
            path = base / terrain.id / name
            if not path.is_file():
                raise RuntimeError(f"{terrain.name} needs its {terrain.dlc} map files. Verify the DayZ installation in Steam.")
            stat = path.stat()
            key = f"{side}/{name}"
            stamp = [str(path.resolve()), stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns, wanted]
            if cached.get(key) != stamp and digest(path) != wanted:
                raise RuntimeError(f"{terrain.name} map files differ from this PREDATE build. Verify the DayZ installation in Steam.")
            checked[key] = stamp
    if checked != cached: atomic_json(cache_path, checked)


def mod_version(folder):
    cpp = folder / "mod.cpp"
    if cpp.exists():
        match = re.search(r'version\s*=\s*"([^"]+)"', cpp.read_text(errors="replace"))
        if match: return match[1]
    if folder.name == "@DabsFramework":
        pbo = next(p for p in folder.rglob("*.pbo") if p.name.lower() == "scripts.pbo")
        match = re.search(rb'DABSFRAMEWORK_([0-9_]+)', pbo.read_bytes())
        if match: return match[1].decode().replace("_", ".")
    return "not declared; identified by file hashes"


def collect(hash_file=None):
    hash_file = hash_file or digest
    server = STEAM / "common/DayZServer"
    client = STEAM / "common/DayZ"
    tools = STEAM / "common/DayZ Tools/Bin"
    version = subprocess.run(["powershell", "-NoProfile", "-Command",
        f"(Get-Item -LiteralPath '{server / 'DayZServer_x64.exe'}').VersionInfo.FileVersion"],
        capture_output=True, text=True, check=True).stdout.strip()
    native = {}
    for folder in ("addons", "dta"):
        native.update({folder + "/" + name: sha for name, sha in
                       fingerprints(server / folder, {".pbo", ".bisign"}, hash_file).items()})
    for p in server.iterdir():
        if p.is_file() and p.suffix.lower() in {".exe", ".dll", ".gproj"}:
            native[p.name] = hash_file(p)
    return {"schema": 1, "dayz_version": version,
            "server": native,
            "client": {name: hash_file(client / name) for name in ("DayZ_x64.exe", "DayZDiag_x64.exe")},
            "tools": {name: hash_file(tools / name) for name in
                      ("PboUtils/FileBank.exe", "DsUtils/DSSignFile.exe", "DsUtils/DSCreateKey.exe")},
            "dependencies": {name: {"workshop_id": item, "version": mod_version(ROOT / "mods" / name),
                "files": fingerprints(ROOT / "mods" / name, hash_file=hash_file)} for name, item in DEPENDENCIES.items()},
            **{item.mission_lock: json.loads((ROOT / "source/mission" / item.base_manifest).read_text())
               for item in maps.TERRAINS.values()}}


def verify(*, launch=False):
    if not LOCK.exists():
        raise RuntimeError("No release lock. Review installed versions, then run py -3 scripts/build.py --accept-dependency-update.")
    expected = json.loads(LOCK.read_text())
    if launch:
        cache = LaunchFingerprints(ROOT / "session-runtime" / "launch-release-fingerprints.json")
        wanted = {}
        for base, entries in ((STEAM / "common/DayZServer", expected["server"]),
                              (STEAM / "common/DayZ", expected["client"]),
                              (STEAM / "common/DayZ Tools/Bin", expected["tools"])):
            wanted.update({base / name: sha for name, sha in entries.items()})
        for name, info in expected["dependencies"].items():
            wanted.update({ROOT / "mods" / name / part: sha for part, sha in info["files"].items()})

        def hash_file(path):
            return cache.fingerprint(path, wanted.get(path))

        actual = collect(hash_file)
    else:
        actual = collect()
    changes = [key for key in actual if actual[key] != expected.get(key)]
    if changes:
        raise RuntimeError("Release inputs changed: " + ", ".join(changes) +
                           ". Review updates explicitly; the build has not replaced dependencies.")
    # Runtime executable/DLL copies must match the locked Steam installation too.
    for name, sha in expected["server"].items():
        if "/" in name:
            continue
        path = ROOT / "runtime" / name
        actual_hash = cache.fingerprint(path, sha) if launch else digest(path)
        if actual_hash != sha:
            raise RuntimeError(f"Runtime does not match the release lock: {name}")
    if launch:
        cache.save()
    return expected


def accept():
    data = collect()
    data["terrain_packages"] = {
        item.id: {"client": terrain_fingerprints(STEAM / "common/DayZ" / item.id),
                  "server": terrain_fingerprints(STEAM / "common/DayZServer" / item.id)}
        for item in maps.TERRAINS.values() if item.dlc}
    write_changed(LOCK, json.dumps(data, indent=2, sort_keys=True) + "\n")
    return data


def source_fingerprint():
    return {"source/" + name: sha for name, sha in fingerprints(ROOT / "source").items()}
