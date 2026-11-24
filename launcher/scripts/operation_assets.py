from pathlib import Path
import argparse
import json
import re
import struct
import zlib

APP = Path(__file__).resolve().parents[1]
RESOURCES = APP / "resources"
ASSETS = APP / "src/renderer/src/assets"
NATIVE_DATA = APP.parent / "source/Predate/data"
MAP_ASSETS = {"chernarusplus": "chernarus", "enoch": "livonia", "sakhal": "sakhal", "nasdara": "nasdara"}
CYRILLIC = dict(zip(
    "абвгдежзийклмнопрстуфхцчшщъыьэюя",
    "a b v g d e zh z i y k l m n o p r s t u f kh ts ch sh shch _ y _ e yu ya".split(),
    strict=True,
))


def latin(text):
    return "".join(
        CYRILLIC.get(c.lower(), c).capitalize() if c.isupper() else CYRILLIC.get(c, c)
        for c in text
    ).replace("_", "")


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def terrain_image(raster):
    size = len(raster)
    pixels = bytearray()
    for y, row in enumerate(raster):
        pixels.append(0)
        for x, height in enumerate(row):
            if height == -9999:
                pixels.extend((9, 22, 40))
                continue
            left, right = row[max(0, x - 1)], row[min(size - 1, x + 1)]
            above = raster[max(0, y - 1)][x]
            below = raster[min(size - 1, y + 1)][x]
            light = max(-23, min(23, (max(0, left) - max(0, right) + max(0, above) - max(0, below)) * .45))
            contour = -7 if height % 40 < 4 else 0
            coast = 12 if min(left, right, above, below) == -9999 else 0
            tone = int(24 + min(510, max(0, height)) * .063 + light + contour + coast)
            pixels.extend((max(11, tone // 2), max(33, tone + 24), max(49, tone + 45)))

    return b"\x89PNG\r\n\x1a\n" + (
        png_chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(pixels, 9))
        + png_chunk(b"IEND", b"")
    )


def mainland_mask(raster):
    size = len(raster)
    remaining = {(row, col) for row in range(size) for col in range(size)
                 if raster[row][col] != -9999}
    mainland = set()
    while remaining:
        first = remaining.pop()
        frontier = [first]
        for row, col in frontier:
            for neighbour in ((row - 1, col), (row + 1, col), (row, col - 1), (row, col + 1)):
                if neighbour in remaining:
                    remaining.remove(neighbour)
                    frontier.append(neighbour)
        if len(frontier) > len(mainland):
            mainland = set(frontier)
    return "\n".join("".join("1" if (row, col) in mainland else "0"
                              for col in range(1, size, 2)) for row in range(1, size, 2)) + "\n"


def main():
    parser = argparse.ArgumentParser(description="Build launcher assets from the native operation setup export")
    parser.add_argument("profile", type=Path)
    parser.add_argument("--map", choices=MAP_ASSETS, default="chernarusplus")
    args = parser.parse_args()
    source = json.loads((args.profile / "SetupExport.json").read_text(encoding="utf-8-sig"))
    rows = (args.profile / "TerrainExport.csv").read_text(encoding="ascii").splitlines()
    raster = [list(map(int, row.split(","))) for row in rows]
    if len(raster) != 512 or any(len(row) != 512 for row in raster):
        raise ValueError("Expected a complete 512×512 terrain export")
    if source.get("world", "chernarusplus").lower() != args.map:
        raise ValueError("The terrain export comes from a different map")

    catalog = {}
    for item in source["items"]:
        identifier = item["id"]
        if not re.fullmatch(r"[a-zA-Z0-9_]+", identifier):
            continue
        if item["name"].startswith(("$", "#")):
            item["name"] = identifier
        for key in ("slots", "accepts", "magazines"):
            item[key] = list(dict.fromkeys(value for value in item[key] if value))
        catalog[identifier] = item
    places = [{
        "name": latin(place["name"]) if args.map in {"chernarusplus", "sakhal"} else place["name"],
        "x": round(place["x"]), "z": round(place["z"]),
        "major": place["kind"] in {"Capital", "City"}
    } for place in source["places"] if place["kind"] in {"Capital", "City", "Village"}]
    items = sorted(catalog.values(), key=lambda item: (item["name"].casefold(), item["id"]))
    if args.map == "chernarusplus":
        (RESOURCES / "operation-catalog.json").write_text(
            '[\n' + ',\n'.join(json.dumps(item, ensure_ascii=False, separators=(',', ':')) for item in items) + '\n]\n',
            encoding="utf-8",
        )
    name = MAP_ASSETS[args.map]
    data = {"size": source["size"], "places": places}
    if args.map != "chernarusplus":
        data.update(sunrise=source["sunrise"], sunset=source["sunset"])
    (RESOURCES / f"{name}-places.json").write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    ASSETS.mkdir(parents=True, exist_ok=True)
    (ASSETS / f"{name}.png").write_bytes(terrain_image(raster))
    if args.map == "sakhal":
        (NATIVE_DATA / "sakhal-mainland.txt").write_text(mainland_mask(raster), encoding="ascii")


if __name__ == "__main__":
    main()
