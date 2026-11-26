"""Build and sign the local FOG quad NOD shadow addon."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory
from urllib.request import urlopen
from zipfile import ZipFile

import build
from quad_nod_shadow_meshes import cross, shadow_parts, unit, validate_part
from quad_nod_shadows import ADDON, ITEM, fingerprints, read_models


CONVERTER_REVISION = "e272780131dc10fe798bad321dca2c96894e993e"
CONVERTER_SHA256 = "5d26ede3a4e831e7d7b90deb2b5aaa7d9b991eb313fca42852b655d647620966"
CONVERTER_URL = f"https://codeload.github.com/SXDIST/DayZObjectBuilder/zip/{CONVERTER_REVISION}"
SHADOW_RESOLUTION = 10_000.0
ADDON_SOURCE = build.ROOT / "source/compat" / ADDON


def provision_converter(work, cache, checkout):
    if checkout is not None:
        revision = subprocess.check_output(["git", "-C", checkout, "rev-parse", "HEAD"], text=True).strip()
        if revision != CONVERTER_REVISION:
            raise RuntimeError(f"Use DayZObjectBuilder commit {CONVERTER_REVISION}.")
        return checkout

    archive = cache / f"DayZObjectBuilder-{CONVERTER_REVISION}.zip"
    data = archive.read_bytes() if archive.is_file() else b""
    if hashlib.sha256(data).hexdigest() != CONVERTER_SHA256:
        print("Downloading the pinned DayZObjectBuilder model converter.", flush=True)
        with urlopen(CONVERTER_URL, timeout=30) as response:
            data = response.read(10_000_000)
        if hashlib.sha256(data).hexdigest() != CONVERTER_SHA256:
            raise ValueError("The DayZObjectBuilder download failed its checksum.")
        download = work / archive.name
        download.write_bytes(data)
        download.replace(archive)
    with ZipFile(io.BytesIO(data)) as source:
        source.extractall(work)
    return work / archive.stem


def shadow_mesh(p3d, name):
    lod = p3d.P3D_LOD()
    bone_ranges = {}
    edges = set()
    for bone, (vertices, faces) in shadow_parts(name):
        validate_part(vertices, faces)
        points, polygons = bone_ranges.setdefault(bone, ([], []))
        start = len(lod.verts)
        points.extend(range(start, start + len(vertices)))
        lod.verts.extend((*point, 0) for point in vertices)
        for face in faces:
            a, b, c = (vertices[index] for index in face)
            normal = unit(cross(tuple(y - x for x, y in zip(a, b)), tuple(y - x for x, y in zip(a, c))))
            indices = [start + index for index in reversed(face)]
            polygons.append(len(lod.faces))
            lod.faces.append([indices, [len(lod.normals)] * 3, [(0, 1)] * 3, "", "", 0])
            lod.normals.append(normal)
            edges.update(tuple(sorted(edge)) for edge in zip(indices, (*indices[1:], indices[0])))
    for bone, (points, polygons) in bone_ranges.items():
        tag = p3d.P3D_TAGG()
        tag.name = bone
        tag.data = p3d.P3D_TAGG_DataSelection()
        tag.data.count_verts = len(lod.verts)
        tag.data.count_faces = len(lod.faces)
        tag.data.weight_verts = [(index, 1.0) for index in points]
        tag.data.weight_faces = [(index, 1.0) for index in polygons]
        lod.taggs.append(tag)
    tag = p3d.P3D_TAGG()
    tag.name = "#SharpEdges#"
    tag.data = p3d.P3D_TAGG_DataSharpEdges()
    tag.data.edges = sorted(edges)
    lod.taggs.append(tag)
    return lod


def compile_shadow_meshes(work, original, converter):
    """Use a temporary visual slot because DayZ Tools strips ShadowVolume slots."""
    project = work / "project"
    source = project / "carriers"
    result = work / "compiled"
    result.mkdir()
    subprocess.run([sys.executable, converter / "tools/debinarize_mod.py", original, source,
                    "--models-only"], check=True)
    from debinarize_mod import p3d

    model_config = source / "model.cfg"
    config_text = model_config.read_text(encoding="utf-8")
    aliases = []
    for path in list(source.glob("*.p3d")):
        model = p3d.P3D_MLOD.read_file(path)
        model.lods = [shadow_mesh(p3d, path.stem), model.find_lod(p3d.P3D_LOD_Resolution.GEOMETRY),
                      model.find_lod(p3d.P3D_LOD_Resolution.MEMORY)]
        model.write_file(path)

        # The local player hides the worn model in first person. An additive
        # black surface renders only its shadow and needs no collision LOD.
        for face in model.lods[0].faces:
            face[3] = "#(argb,8,8,3)color(0,0,0,1,ca)"
            face[4] = f"{ADDON.lower()}\\data\\caster.rvmat"
        # Keep the source origin without carrying over its collision mesh.
        geometry = model.lods[1]
        empty_geometry = p3d.P3D_LOD()
        empty_geometry.resolution = geometry.resolution
        empty_geometry.taggs = [tag for tag in geometry.taggs if tag.name == "#Property#"]
        model.lods[1] = empty_geometry
        caster = path.stem + "_Caster"
        model.write_file(source / (caster + ".p3d"))
        aliases.append(f"\tclass {caster}: {path.stem} {{}};")
    end = config_text.rindex("};")
    model_config.write_text(config_text[:end] + "\n".join(aliases) + "\n" + config_text[end:],
                            encoding="utf-8")
    shutil.copytree(ADDON_SOURCE / "data", project / ADDON / "data")
    tools = build.TOOLS / "Binarize"
    shutil.copy2(tools / "bin.txt", project)
    shutil.copytree(tools / "bin", project / "bin")
    subprocess.run([tools / "binarize.exe", "-always", str(source) + "\\", str(result) + "\\", "*.p3d"],
                   cwd=project, check=True)
    return result


def insert_shadow_lod(reader, data, mesh):
    """Append the compiled mesh payload while retaining every original LOD byte."""
    stream = io.BytesIO(data)
    try:
        model = reader.ODOL_File.read(stream)
    except reader.ODOL_Error as error:
        raise ValueError(f"Could not read the installed quad NOD model: {error}") from error
    count = len(model.resolutions)
    if (model.version != 54 or model.offset or model.failed_lods or model.bones != ["head", "rotate"]
            or not any(value > SHADOW_RESOLUTION for value in model.resolutions)):
        raise ValueError("The installed quad NOD has an unsupported ODOL layout.")
    insert = next(i for i, value in enumerate(model.resolutions) if value > SHADOW_RESOLUTION)
    if SHADOW_RESOLUTION in model.resolutions:
        raise ValueError("The installed quad NOD already has a ShadowVolume LOD.")

    # ODOL 54 model info keeps special LOD indices after the skeleton and masses.
    model_info = 12 + 4 * count
    shadow_source = model_info + 202
    stream.seek(shadow_source + 10)
    reader.read_skeleton(stream, model.version, len(data))
    stream.read(1)
    reader.read_compressed_floats(stream, len(data))
    stream.read(16)
    special_indices = stream.tell()
    stream.seek(model_info)
    reader.read_model_info(stream, model.version, len(data))

    # The existing model has one rotate animation. Copy its mapping and axis
    # from the highest-detail visual LOD into the new shadow slot.
    if stream.read(5) != b"\x01\x01\x00\x00\x00":
        raise ValueError("Expected exactly one quad NOD animation.")
    stream.read(4)
    if (reader.binary.read_asciiz(stream), reader.binary.read_asciiz(stream)) != ("rotate", "rotate"):
        raise ValueError("Expected the original quad NOD hinge animation.")
    stream.read(28)
    mapping_count = stream.tell()
    if reader.binary.read_ulong(stream) != count:
        raise ValueError("Expected a bone mapping for every original LOD.")
    mappings = []
    for _ in range(count):
        start = stream.tell()
        bone_count = reader.read_count(stream, len(data), "bone")
        for _ in range(bone_count):
            stream.read(4 * reader.read_count(stream, len(data), "bone animation"))
        mappings.append(data[start:stream.tell()])
    axes = []
    for _ in range(count):
        start = stream.tell()
        if reader.binary.read_long(stream) != -1:
            stream.read(24)
        axes.append(data[start:stream.tell()])
    original_table = struct.pack(f"<{count * 2}I", *model.lod_starts, *model.lod_ends) + bytes(model.permanent)
    if stream.read(len(original_table)) != original_table or max(model.lod_ends) != len(data):
        raise ValueError("Unexpected quad NOD LOD address table.")

    header = bytearray(data[:mapping_count])
    struct.pack_into("<I", header, 8, count + 1)
    struct.pack_into("<I", header, shadow_source, 2)
    header[shadow_source + 4] = 1
    header[special_indices:special_indices + 13] = bytes(
        255 if index == 255 else index + (index >= insert)
        for index in header[special_indices:special_indices + 13]
    )
    struct.pack_into("<I", header, special_indices + 13, insert)
    header[12 + 4 * insert:12 + 4 * insert] = struct.pack("<f", SHADOW_RESOLUTION)

    growth = 4 + len(mappings[0]) + len(axes[0]) + 9
    mappings.insert(insert, mappings[0])
    axes.insert(insert, axes[0])
    starts = [start + growth for start in model.lod_starts]
    ends = [end + growth for end in model.lod_ends]
    starts.insert(insert, len(data) + growth)
    ends.insert(insert, starts[insert] + len(mesh))
    permanent = list(model.permanent)
    permanent.insert(insert, True)
    compiled = (bytes(header) + struct.pack("<I", count + 1) + b"".join(mappings + axes)
                + struct.pack(f"<{(count + 1) * 2}I", *starts, *ends) + bytes(permanent)
                + data[stream.tell():] + mesh)
    parsed = reader.ODOL_File.read(io.BytesIO(compiled))
    if (parsed.failed_lods or parsed.resolutions[insert] != SHADOW_RESOLUTION
            or parsed.animations.classes[0].axes[insert] != model.animations.classes[0].axes[0]
            or len(compiled) >= 100_000_000):
        raise ValueError("The compiled model lost its original hinge or shadow LOD.")
    for index, (start, end) in enumerate(zip(model.lod_starts, model.lod_ends)):
        moved = index + (index >= insert)
        if data[start:end] != compiled[parsed.lod_starts[moved]:parsed.lod_ends[moved]]:
            raise ValueError("The compiled model changed an original LOD.")
    return compiled


def pack(source, mod, gear, models, recipe, authority, public_key, staging):
    pbo, signature = build.stage_signed_addon(source, staging, authority)
    key = staging / public_key.name
    shutil.copy2(public_key, key)
    manifest = staging / "models.json"
    manifest.write_text(json.dumps(models, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    packages = {f"addons/{pbo.name}": pbo, f"addons/{signature.name}": signature,
                f"keys/{key.name}": key, "models.json": manifest}
    record = staging / "build.json"
    record.write_text(json.dumps({
        "recipe": recipe,
        "models": models,
        "files": {name: build.release.digest(path) for name, path in packages.items()},
    }, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if fingerprints(gear) != models:
        raise RuntimeError("Forward Operator Gear changed during the shadow build. Let Steam finish and rerun setup.")
    build.publish_packages(
        [(path, mod / name) for name, path in packages.items()] + [(record, mod / record.name)],
        staging,
    )


def package_matches(mod, models, recipe, public_key, authority):
    paths = (f"addons/{ADDON}.pbo", f"addons/{ADDON}.pbo.{authority}.bisign",
             f"keys/{public_key.name}", "models.json")
    try:
        record = json.loads((mod / "build.json").read_text(encoding="utf-8"))
        return (isinstance(record, dict) and record.get("models") == models and record.get("recipe") == recipe
                and record.get("files") == {name: build.release.digest(mod / name) for name in paths})
    except (OSError, ValueError):
        return False


def build_package(gear, mod, checkout=None):
    build.require_closed_runtime()
    authority, public_key = build.local_signing_authority()
    assets = dict(read_models(gear))
    models = {name: hashlib.sha256(data).hexdigest() for name, data in assets.items()}
    binarize = build.TOOLS / "Binarize"
    recipe = {
        "converter": CONVERTER_SHA256,
        "builder": build.release.digest(__file__),
        "meshes": build.release.digest(Path(__file__).with_name("quad_nod_shadow_meshes.py")),
        "addon_source": build.release.fingerprints(ADDON_SOURCE),
        "public_key": build.release.digest(public_key),
        "binarize": build.release.fingerprints(binarize),
    }
    if package_matches(mod, models, recipe, public_key, authority):
        print("Quad NOD shadow addon already matches Forward Operator Gear.", flush=True)
        return

    work_root = build.ROOT / "build"
    work_root.mkdir(parents=True, exist_ok=True)
    mod.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix=".pack-", dir=mod) as directory:
        work = Path(directory)
        converter = provision_converter(work, work_root, checkout)
        source = compile_models(work, assets, converter)
        shutil.copytree(ADDON_SOURCE, source, dirs_exist_ok=True)
        pack(source, mod, gear, models, recipe, authority, public_key, work)
    print(f"Built and signed {ADDON} in {mod}.", flush=True)


def compile_models(work, assets, converter):
    sys.path.insert(0, str(converter / "tools"))
    try:
        from debinarize_mod import odol_reader

        original, source = work / "original", work / ADDON
        original.mkdir()
        source.mkdir()
        for name, data in assets.items():
            (original / (name + ".p3d")).write_bytes(data)
        carriers = compile_shadow_meshes(work, original, converter)
        for name, data in assets.items():
            carrier = (carriers / (name + ".p3d")).read_bytes()
            parsed = odol_reader.ODOL_File.read(io.BytesIO(carrier))
            triangles = sum(len(faces) for _, (_, faces) in shadow_parts(name))
            if (parsed.failed_lods or parsed.resolutions[0] != 0 or len(parsed.lods[0].faces) != triangles
                    or {selection.name for selection in parsed.lods[0].named_selections} != {"head", "rotate"}):
                raise ValueError(f"Shadow mesh or bone binding was lost while compiling {name}.")
            payload = carrier[parsed.lod_starts[0]:parsed.lod_ends[0]]
            compiled = insert_shadow_lod(odol_reader, data, payload)
            (source / (name + ".p3d")).write_bytes(compiled)
            caster = name + "_Caster.p3d"
            caster_data = (carriers / caster).read_bytes()
            parsed_caster = odol_reader.ODOL_File.read(io.BytesIO(caster_data))
            if (parsed_caster.failed_lods or parsed_caster.bones != parsed.bones
                    or len(parsed_caster.lods[0].faces) != triangles
                    or len(parsed_caster.animations.classes) != 1
                    or parsed_caster.animations.classes[0].axes[0] != parsed.animations.classes[0].axes[0]):
                raise ValueError(f"First-person shadow mesh or hinge was lost while compiling {name}.")
            (source / caster).write_bytes(caster_data)
            print(f"{name}.p3d: {len(compiled):,} bytes, {triangles} shadow triangles", flush=True)
        return source
    finally:
        sys.path.pop(0)


def install_if_available():
    gear = build.WORKSHOP / ITEM / "Addons/Gear.pbo"
    if not gear.is_file():
        print("Quad NOD shadow build skipped. Install Forward Operator Gear in Steam and rerun setup.", flush=True)
        return
    try:
        build_package(gear, build.ROOT / "mods" / ("@" + ADDON))
    except (OSError, RuntimeError, ValueError, KeyError, EOFError, struct.error, subprocess.SubprocessError) as error:
        print(f"Quad NOD shadow build unavailable: {error}", flush=True)
        print("Hunts can use Forward Operator Gear's original models. Rerun setup to retry.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--converter", type=Path, help="Use an existing checkout of the pinned converter.")
    parser.add_argument("--output", type=Path, default=build.ROOT / "mods" / ("@" + ADDON))
    args = parser.parse_args()
    checkout = args.converter.resolve() if args.converter else None
    build_package(build.WORKSHOP / ITEM / "Addons/Gear.pbo", args.output.resolve(), checkout)


if __name__ == "__main__":
    main()
