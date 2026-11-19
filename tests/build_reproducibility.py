"""Run two real builds and assert source tools, worlds, dependencies and package stay stable."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import release


def snapshot():
    paths = [ROOT / "world", ROOT / "worlds"]
    paths += [ROOT / "mods" / name for name in release.DEPENDENCIES]
    data = {p.relative_to(ROOT).as_posix(): release.fingerprints(p) for p in paths}
    data["scripts/world.py"] = release.digest(ROOT / "scripts/world.py")
    return data


def main():
    baseline = snapshot()
    packages = []
    for _ in range(2):
        subprocess.run([sys.executable, str(ROOT / "scripts/build.py")], check=True, cwd=ROOT)
        assert snapshot() == baseline, "Build changed a world, dependency or authored manager"
        packages.append(release.fingerprints(ROOT / "mods/@Predate"))
    assert packages[0] == packages[1], "Unchanged build inputs changed the signed addon"
    release.write_changed(ROOT / "build/reproducibility-results.json", json.dumps({
        "schema": 1, "two_builds_identical": True,
        "worlds_dependencies_manager_unchanged": True,
        "package": packages[1]}, indent=2) + "\n")
    print("PASS: two builds preserved all worlds/dependencies/tools and the identical signed package.")


if __name__ == "__main__": main()
