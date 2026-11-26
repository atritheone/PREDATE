"""Release inputs and fingerprints; no downloads, saves or credentials are managed here."""
from pathlib import Path
import hashlib
import json
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


def write_changed(path, data):
    path = Path(path)
    if isinstance(data, str): data = data.encode("utf-8")
    if not path.exists() or path.read_bytes() != data:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def fingerprints(folder, suffixes=None):
    return {p.relative_to(folder).as_posix(): digest(p) for p in sorted(folder.rglob("*"))
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


def collect():
    server = STEAM / "common/DayZServer"
    client = STEAM / "common/DayZ"
    tools = STEAM / "common/DayZ Tools/Bin"
    version = subprocess.run(["powershell", "-NoProfile", "-Command",
        f"(Get-Item -LiteralPath '{server / 'DayZServer_x64.exe'}').VersionInfo.FileVersion"],
        capture_output=True, text=True, check=True).stdout.strip()
    native = {}
    for folder in ("addons", "dta"):
        native.update({folder + "/" + name: sha for name, sha in
                       fingerprints(server / folder, {".pbo", ".bisign"}).items()})
    for p in server.iterdir():
        if p.is_file() and p.suffix.lower() in {".exe", ".dll", ".gproj"}:
            native[p.name] = digest(p)
    return {"schema": 1, "dayz_version": version,
            "server": native,
            "client": {name: digest(client / name) for name in ("DayZ_x64.exe", "DayZDiag_x64.exe")},
            "tools": {name: digest(tools / name) for name in
                      ("PboUtils/FileBank.exe", "DsUtils/DSSignFile.exe", "DsUtils/DSCreateKey.exe")},
            "dependencies": {name: {"workshop_id": item, "version": mod_version(ROOT / "mods" / name),
                "files": fingerprints(ROOT / "mods" / name)} for name, item in DEPENDENCIES.items()},
            **{item.mission_lock: json.loads((ROOT / "source/mission" / item.base_manifest).read_text())
               for item in maps.TERRAINS.values()}}


def verify():
    if not LOCK.exists():
        raise RuntimeError("No release lock. Review installed versions, then run py -3 scripts/build.py --accept-dependency-update.")
    expected = json.loads(LOCK.read_text())
    actual = collect()
    changes = [key for key in actual if actual[key] != expected.get(key)]
    if changes:
        raise RuntimeError("Release inputs changed: " + ", ".join(changes) +
                           ". Review updates explicitly; the build has not replaced dependencies.")
    # Runtime executable/DLL copies must match the locked Steam installation too.
    for name, sha in expected["server"].items():
        if "/" not in name and digest(ROOT / "runtime" / name) != sha:
            raise RuntimeError(f"Runtime does not match the release lock: {name}")
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
