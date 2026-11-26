"""Resolve the local quad NOD addon against the Workshop models it was built for."""

from dataclasses import dataclass
import hashlib
import json
import struct


ITEM = "2931560672"
ADDON = "PredateQuadNodShadows"
MODEL_PATHS = {
    "FOG_GPNVG": "nvg\\gpnvg\\fog_gpnvg.p3d",
    "FOG_GPNVG_GSGM": "nvg\\gpnvg\\fog_gpnvg_gsgm.p3d",
    "GPNVG_Elite": "nvg\\gpnvg_elite\\gpnvg_elite.p3d",
}


@dataclass(frozen=True)
class PboEntry:
    offset: int
    size: int
    method: int


def read_string(stream):
    value = bytearray()
    while True:
        char = stream.read(1)
        if not char:
            raise EOFError("PBO header ended before its file table.")
        if char == b"\x00":
            return value.decode("utf-8")
        value.extend(char)


def index_pbo(stream):
    files = []
    while True:
        name = read_string(stream)
        method, _, _, _, stored_size = struct.unpack("<5I", stream.read(20))
        if name:
            files.append((name, stored_size, method))
        elif method == 0x56657273:
            while read_string(stream):
                read_string(stream)
        elif method == 0:
            break
        else:
            raise ValueError("Unknown PBO header format.")

    offset = stream.tell()
    entries = {}
    for name, size, method in files:
        entries[name.casefold()] = PboEntry(offset, size, method)
        offset += size
    return entries


def read_models(archive):
    with archive.open("rb") as stream:
        entries = index_pbo(stream)
        for name, path in MODEL_PATHS.items():
            entry = entries[path]
            if entry.method or entry.size >= 100_000_000:
                raise ValueError("Unsupported quad NOD PBO entry.")
            stream.seek(entry.offset)
            data = stream.read(entry.size)
            if len(data) != entry.size:
                raise EOFError("PBO entry was truncated.")
            yield name, data


def fingerprints(archive):
    return {name: hashlib.sha256(data).hexdigest() for name, data in read_models(archive)}


def companion(root, mods):
    folder = root / "mods" / ("@" + ADDON)
    pbo = folder / "addons" / (ADDON + ".pbo")
    signature = pbo.with_name(pbo.name + ".PredateLocal.bisign")
    if not pbo.is_file() or not signature.is_file():
        return None
    try:
        manifest = json.loads((folder / "models.json").read_text(encoding="utf-8"))
        for mod in mods:
            if mod.name != ITEM:
                continue
            if fingerprints(mod / "Addons/Gear.pbo") == manifest:
                return folder
    except (OSError, ValueError, KeyError, EOFError, struct.error):
        return None
    return None
