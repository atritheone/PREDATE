"""Release inputs and fingerprints; no downloads, saves or credentials are managed here."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

from app_runtime import ROOT
STEAM = Path(r"C:\Program Files (x86)\Steam\steamapps")
LOCK = ROOT / "config/release-lock.json"
DEPENDENCIES = {"@CF": "1559212036", "@DabsFramework": "2545327648",
                "@ExpansionCore": "2291785308", "@ExpansionAI": "2792982069"}


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
            "client": {"DayZ_x64.exe": digest(STEAM / "common/DayZ/DayZ_x64.exe")},
            "tools": {name: digest(tools / name) for name in
                      ("PboUtils/FileBank.exe", "DsUtils/DSSignFile.exe", "DsUtils/DSCreateKey.exe")},
            "dependencies": {name: {"workshop_id": item, "version": mod_version(ROOT / "mods" / name),
                "files": fingerprints(ROOT / "mods" / name)} for name, item in DEPENDENCIES.items()},
            "mission_base": json.loads((ROOT / "source/mission/base-files.json").read_text())}


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
    write_changed(LOCK, json.dumps(data, indent=2, sort_keys=True) + "\n")
    return data


def source_fingerprint():
    return {"source/" + name: sha for name, sha in fingerprints(ROOT / "source").items()}
