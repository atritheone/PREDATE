"""Select the tested Workshop laser pack and its required local companion."""
import hashlib
from pathlib import Path


ITEM_ID = "3704871552"
WORKSHOP_PBO = "mutatedlasers.pbo"
COMPANION_PBO = "predatemutatedlasers.pbo"
COMPANION_FOLDER = "@PredateMutatedLasers"
TESTED_SHA256 = "97958b9583aa7b4edf4e3b5348669026f9894159d975de7fc28a7981a715215b"


def ready_pair(root, workshop):
    folders = workshop / ITEM_ID, root / "mods" / COMPANION_FOLDER
    for folder, expected in zip(folders, (WORKSHOP_PBO, COMPANION_PBO)):
        if not any(pbo.name.casefold() == expected for pbo in folder.glob("[Aa]ddons/*.pbo")):
            return []
    return list(folders)


def preview_available(root, workshop):
    pair = ready_pair(root, workshop)
    if not pair:
        return False
    try:
        verify_launch(pair)
    except (OSError, ValueError):
        return False
    return True


def verify_pair(lock):
    workshop, companion = [], []
    for index, addon in enumerate(lock["mods"]):
        for path, digest in addon["files"].items():
            name = Path(path).name.casefold()
            if name == WORKSHOP_PBO:
                workshop.append((index, digest))
            if name == COMPANION_PBO:
                companion.append(index)
    if not workshop and not companion:
        return
    if len(workshop) != 1 or len(companion) != 1 or companion[0] <= workshop[0][0]:
        raise ValueError("MutatedMods Lasers requires the PREDATE compatibility addon immediately after the Workshop pack or later in the mod order.")
    if workshop[0][1] != TESTED_SHA256:
        raise ValueError("MutatedMods Lasers changed on Steam. This release must be checked with the PREDATE compatibility addon before it can be loaded.")


def verify_launch(paths):
    """Check the actual mod list before spawning either DayZ executable."""
    entries = []
    for folder in paths:
        files = {}
        for pbo in Path(folder).glob("[Aa]ddons/*.pbo"):
            name = pbo.name.casefold()
            if name == WORKSHOP_PBO:
                with pbo.open("rb") as stream:
                    files[pbo.name] = hashlib.file_digest(stream, "sha256").hexdigest()
            elif name == COMPANION_PBO:
                if not pbo.with_name(pbo.name + ".PredateLocal.bisign").is_file():
                    raise ValueError("The Workshop laser compatibility addon has no PREDATE signature. Rebuild PREDATE before using the laser pack.")
                files[pbo.name] = ""
        if files:
            entries.append({"files": files})
    verify_pair({"mods": entries})
