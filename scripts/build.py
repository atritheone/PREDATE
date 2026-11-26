"""Build and sign the local PREDATE prototype using the installed DayZ tools."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import argparse
from tempfile import TemporaryDirectory
import release

ROOT = Path(__file__).resolve().parents[1]
INSTALL = Path(r"C:\Program Files (x86)\Steam\steamapps\common\DayZServer")
TOOLS = Path(r"C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin")
WORKSHOP = release.WORKSHOP

# Match PredateProjectileRounds.SLOT_COUNT.
ROUND_SLOTS = 192
ROUND_AMMO = {
    "PredateStockPrecision": (
        "Bullet_12GaugePellets", "Bullet_12GaugeSlug", "Bullet_12GaugeRubberSlug",
        "Bullet_12GaugeBeanbag", "Bullet_556x45", "Bullet_556x45Tracer",
        "Bullet_545x39", "Bullet_545x39Tracer", "Bullet_762x54", "Bullet_762x54Tracer",
        "Bullet_308Win", "Bullet_308WinTracer", "Bullet_762x39", "Bullet_762x39Tracer",
        "Bullet_45ACP", "Bullet_357", "Bullet_9x19", "Bullet_380", "Bullet_22",
        "Bullet_9x39AP", "Bullet_9x39",
    ),
    "PredateBucaPrecision": tuple(
        "BC_Bullet_" + caliber for caliber in (
            "127x55", "20mm", "300BO", "338", "408", "408AP", "46x30mm",
            "50Beowulf", "50Cal", "FN57x28mm", "Rail",
        )
    ),
}


def require_closed_runtime():
    """Protect loaded addons and missions even when a build helper is imported."""
    import platform_win
    for name in ("DayZ_x64.exe", "DayZServer_x64.exe", "DayZDiag_x64.exe"):
        if platform_win.find_processes(name):
            raise RuntimeError(f"Close {name} before replacing runtime, mission or addon files.")


def run(args, cwd=None):
    result = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    if result.stdout.strip(): print(result.stdout.strip(), flush=True)


def copy_changed(src, dst):
    source, target = Path(src), Path(dst)
    if target.exists() and source.stat().st_size == target.stat().st_size and source.stat().st_mtime_ns == target.stat().st_mtime_ns:
        return str(target)
    return shutil.copy2(source,target)


def widget(kind, name, x, y, w, h, extra="", children=""):
    attributes = f" {extra}" if extra else ""
    return f'''{kind} {name} {{
 position {x} {y}
 size {w} {h}
 hexactpos 0
 vexactpos 0
 hexactsize 0
 vexactsize 0
{attributes}
 {{ {children} }}
}}
'''


def label(name, text, x, y, w, h, color="0.9 0.93 0.95 1"):
    return widget("TextWidgetClass", name, x,y,w,h,
                  f'ignorepointer 1\ncolor {color}\ntext "{text}"\nfont "gui/fonts/etelkatextpro22"\n"text valign" center')


def map_label(name, color):
    return widget("TextWidgetClass", name, 0,0,.12,.035,
                  f'visible 0\nignorepointer 1\npriority 120\ncolor {color}\nscaled 0\nfont "gui/fonts/sdf_MetronBook24"\n'
                  '"exact text" 1\n"exact text size" 15\n"bold text" 1\n"text halign" center\n"text valign" center')


def map_image(name, image, x=0, y=0, w=1, h=1, extra=""):
    return widget("ImageWidgetClass", name, x, y, w, h,
                  f'ignorepointer 1\nscaled 0\nmode blend\n"src alpha" 1\n'
                  f'"stretch mode" stretch_w_h\nfilter 1\nimage0 "{image}"\n{extra}')


def layouts():
    gui = ROOT / "source/Predate/gui"
    gui.mkdir(exist_ok=True)
    history_canvas = widget("CanvasWidgetClass", "MapHistoryGuides", 0,0,1,1,"ignorepointer 1\npriority 80")
    canvas = widget("CanvasWidgetClass", "MapOverlay", 0,0,1,1,"ignorepointer 1\npriority 110")
    bearing_labels = "".join(map_label(f"MapBearing{index}", "0.70 0.85 0.95 1") for index in range(24))
    pulse_labels = "".join(map_label(f"MapPulse{index}", "1 0.79 0.44 1") for index in range(100))
    historical_label = map_label("MapLastKnown", "0.66 0.88 0.90 1")
    arrow = map_image("MapPlayer", "set:PredateMapIcons image:Player", extra="visible 0\npriority 130")
    # Widget edges are measured from the original 1672 by 941 tablet art.
    children = widget("MapWidgetClass", "Terrain", 75/1672,69/941,1028/1672,805/941, 'clipchildren 1',
                      history_canvas+canvas+bearing_labels+pulse_labels+historical_label+arrow)
    children += map_image("TabletArtwork", "set:PredateMapTablet image:Tablet", extra="priority 150")
    for name, glyph in (("Overview", "Overview"), ("CenterPlayer", "Center")):
        icon = map_image(name+"Icon", f"set:PredateMapIcons image:{glyph}", .2,.2,.6,.6)
        children += widget("ButtonWidgetClass", name, 0,0,.035,.055,
                           'priority 200\nstyle PredateMapControl', icon)
    header_inset = 3 / 805
    readout = label("TrackingTitle", "TRACKING", .06,header_inset,.50,.053,"0.72 0.72 0.72 1")
    readout = readout.replace('"text valign" center', '"text valign" top')
    readout += label("Status", "Waiting for pulses", .06,.084,.88,.047)
    readout += label("TargetName", "", .06,.084,.88,.047).replace(
        'ignorepointer 1', 'ignorepointer 1\nvisible 0\n"bold text" 1')
    readout += label("Bearing", "No pulse", .06,.145,.88,.042)
    readout += label("Timing", "", .06,.20,.88,.04)
    readout += label("Hint", "", .06,.26,.88,.04)
    readout += label("InterceptionStatus", "HUNTED", .62,header_inset,.32,.053,"1 0.698039 0.325490 1").replace(
        '"text valign" center', '"text valign" top\n"text halign" right').replace(
        'ignorepointer 1','ignorepointer 1\nvisible 0\n"bold text" 1')
    readout += label("HistoryTitle", "PULSES", .06,.50,.88,.048,
                     "0.72 0.72 0.72 1").replace('ignorepointer 1','ignorepointer 1\nvisible 0')
    for index in range(4):
        row = label(f"History{index}", "", .06,.548 + index * .044,.92,.043)
        readout += row.replace('ignorepointer 1','ignorepointer 1\nvisible 0')
    readout = readout.replace('font "gui/fonts/etelkatextpro22"',
                              'scaled 0\nfont "gui/fonts/sdf_MetronBook24"\n"exact text" 1\n"exact text size" 15')
    pulse_picker = label("PulsePickerTitle", "Overlapping pulses", 0,0,1,.14,"0.55 0.85 0.90 1")
    pulse_picker = pulse_picker.replace('font "gui/fonts/etelkatextpro22"',
                                        'scaled 0\nfont "gui/fonts/sdf_MetronBook24"\n"exact text" 1\n"exact text size" 15')
    pulse_picker += widget("TextListboxWidgetClass","PulseChoices",0,.16,1,.84,
                           'lines 9\nfont "gui/fonts/sdf_MetronBook24"')
    readout += widget("FrameWidgetClass","PulsePicker",.06,.48,.88,.40,'visible 0',pulse_picker)
    readout += widget("ButtonWidgetClass","Select",.06,.366,.24,.041,
                       'visible 0\nstyle PredateMapClear\ntext "Clear Lock"')
    readout += widget("ButtonWidgetClass","NewHunt",.06,.913,.52,.058,
                       'visible 0\nstyle Default\ntext "Reset test range"')
    readout += widget("ButtonWidgetClass","Deploy",.06,.366,.42,.057,
                       'visible 0\nstyle Default\ntext "Start another hunt"')
    readout += widget("ButtonWidgetClass","ConfirmDeploy",.52,.366,.42,.057,
                       'visible 0\nstyle Default\ntext "Confirm hunt"')
    readout += widget("ButtonWidgetClass","Close",.758,.9177,.182,.0533,
                       'style Default\ntext "Close"')
    children += widget("FrameWidgetClass","Readout",1119/1672,69/941,479/1672,805/941,
                       'priority 200',readout)
    panel = widget("FrameWidgetClass","Panel",.035,.035,.93,.93,"ignorepointer 1",children)
    release.write_changed(gui/"tracker.layout",widget("FrameWidgetClass","PredateTracker",0,0,1,1,children=panel))
    directions={0:"N",45:"NE",90:"E",135:"SE",180:"S",225:"SW",270:"W",315:"NW"}
    ticks=""
    for i in range(144):
        degrees=(i*5-180) % 360
        parts=""
        if degrees % 15 == 0:
            text=directions.get(degrees,f"{degrees:03d}")
            color="0.70 0.90 0.81 1" if degrees in directions else "0.76 0.80 0.83 1"
            parts=label("CompassLabel"+str(i),text,0,.15,1,.55,color)
            parts=parts.replace('"text valign" center','"text valign" center\n"text halign" center')
        mark_height=.26 if degrees % 45 == 0 else (.16 if degrees % 15 == 0 else .08)
        parts+=widget("PanelWidgetClass","Mark"+str(i),.48,1-mark_height,.04,mark_height,
                      'ignorepointer 1\ncolor 0.70 0.76 0.79 0.85\nstyle UIDefaultPanel')
        ticks+=widget("FrameWidgetClass","Tick"+str(i),i/144-.01,0,.02,1,"ignorepointer 1",parts)
    marker=label("PulseMarker","v",.47,.05,.06,.28,"1 0.70 0.33 1")
    marker=marker.replace('"text valign" center','"text valign" center\n"text halign" center')
    scroll=widget("FrameWidgetClass","CompassScroll",-.5,0,4,1,"ignorepointer 1",ticks)
    sector=widget("PanelWidgetClass","PulseSector",0,.04,.0833,.09,
                  'ignorepointer 1\nvisible 0\ncolor 1 0.70 0.33 1\nstyle rover_sim_colorable')
    tape=widget("FrameWidgetClass","CompassTape",0,.29,1,.43,'ignorepointer 1\nclipchildren 1',scroll+sector+marker)
    heading=label("Heading","N  000 deg",.35,0,.30,.25,"0.92 0.95 0.96 1")
    heading=heading.replace('"text valign" center','"text valign" center\n"text halign" center')
    notch=widget("PanelWidgetClass","HeadingNotch",.4985,.71,.003,.065,
                 'ignorepointer 1\ncolor 0.92 0.95 0.96 1\nstyle UIDefaultPanel')
    receiver=label("Receiver","Map  M",0,.77,1,.23,"0.77 0.83 0.85 1")
    receiver=receiver.replace('"text valign" center','"text valign" center\n"text halign" center')
    backdrop=widget("PanelWidgetClass","CompassBackdrop",0,0,1,.77,
                     'ignorepointer 1\nscaled 0\ncolor 0.02 0.03 0.04 0.40\nstyle PredateCompassBackdrop')
    outline = widget("PanelWidgetClass", "FinalApproachOutline",0,0,1,.77,
                     'ignorepointer 1\nscaled 0\nvisible 0\npriority 10\ncolor 0.95 0.16 0.20 1\nstyle PredateCompassOutline')
    hunted_style = 'ignorepointer 1\ncolor 1 0.698039 0.325490 1\nstyle rover_sim_colorable'
    hunted_lines = widget("PanelWidgetClass", "HuntedLeft", .16,0,.24,.02,hunted_style)
    hunted_lines += widget("PanelWidgetClass", "HuntedRight", .60,0,.24,.02,hunted_style)
    hunted = widget("FrameWidgetClass", "HuntedIndicator", 0,0,1,1,
                    'ignorepointer 1\nvisible 0\npriority 11', hunted_lines)
    compass_parts=backdrop+heading+tape+notch+receiver+outline+hunted
    compass_parts=compass_parts.replace('font "gui/fonts/etelkatextpro22"', 'font "gui/fonts/etelkatextpro22"\n"exact text" 1\n"exact text size" 11')
    compass_parts=compass_parts.replace('font "gui/fonts/etelkatextpro22"', 'scaled 0\nfont "gui/fonts/sdf_MetronBook24"')
    compass=widget("FrameWidgetClass","Compass",.36,.018,.28,.06,"ignorepointer 1",compass_parts)
    footprint = label("FootprintClue", "", .35,.77,.30,.03,"0.77 0.83 0.85 1").replace(
        'ignorepointer 1', 'ignorepointer 1\nvisible 0\n"text halign" center').replace(
        'font "gui/fonts/etelkatextpro22"', 'scaled 0\nfont "gui/fonts/sdf_MetronBook24"\n"exact text" 1\n"exact text size" 14')
    for name in ("FootprintMark", "OwnFootprintMark"):
        footprint += widget("ImageWidgetClass", name, 0,0,.01,.01,
                            'ignorepointer 1\nvisible 0\nmode blend\n"src alpha" 1\n"stretch mode" stretch_w_h\nfilter 1')
    notices = ""
    for index in range(6):
        notices += label(f"HuntNotice{index}", "", 0,index/6,1,1/6,"1 1 1 1").replace(
            'ignorepointer 1', 'ignorepointer 1\nvisible 0\n"text halign" right').replace(
            'font "gui/fonts/etelkatextpro22"', 'scaled 0\nfont "gui/fonts/sdf_MetronBook24"\n"exact text" 1\n"exact text size" 16')
    feed = widget("FrameWidgetClass","HuntFeed",.675,.035,.30,.21,"ignorepointer 1",notices)
    release.write_changed(gui/"hunt_feed.layout",widget("FrameWidgetClass","PredateHuntFeed",0,0,1,1,"ignorepointer 1\nvisible 0\npriority 900",feed))
    release.write_changed(gui/"hud.layout",widget("FrameWidgetClass","PredateHud",0,0,1,1,"ignorepointer 1",compass+footprint))
    death = widget("ImageWidgetClass", "DeathTitle", 0, 0, 1, 1,
                   'ignorepointer 1\ncolor 0 0 0 1\nimage0 "Predate/gui/death_title_ca.paa"\n'
                   'mode blend\n"src alpha" 1\n"stretch mode" stretch_w_h\nfilter 1')
    death_buttons = widget("ButtonWidgetClass","DeathRedeploy",.40,.595,.20,.052,
                           'style Default\ntext "Redeploy"')
    death_buttons += widget("ButtonWidgetClass","DeathSaveQuit",.40,.66,.20,.052,
                            'style Default\ntext "Save and Quit"')
    death += widget("FrameWidgetClass","DeathChoices",0,0,1,1,
                    'visible 0\nignorepointer 1\npriority 10',death_buttons)
    release.write_changed(gui/"death.layout",widget("PanelWidgetClass","PredateDeath",0,0,1,1,
        "visible 0\npriority 9999\ncolor 1 0 0 1\nstyle rover_sim_colorable",death))


def stage_dependencies(dependencies, locked=None):
    if not dependencies:
        return
    require_closed_runtime()
    for name, item in dependencies.items():
        source = WORKSHOP / item
        if not list(source.glob("[Aa]ddons/*.pbo")):
            raise RuntimeError(f"Workshop dependency is not installed: {name} ({item})")
        if locked is not None and release.fingerprints(source) != locked[name]["files"]:
            raise RuntimeError(f"Workshop dependency does not match the release lock: {name} ({item})")

    for name, item in dependencies.items():
        source = WORKSHOP / item
        target = ROOT / "mods" / name
        target.mkdir(parents=True, exist_ok=True)
        # Stage an exact snapshot so files removed by an upstream update cannot linger.
        source_files = {p.relative_to(source) for p in source.rglob("*") if p.is_file()}
        for existing in target.rglob("*"):
            if existing.is_file() and existing.relative_to(target) not in source_files:
                existing.unlink()
        shutil.copytree(source, target, dirs_exist_ok=True, copy_function=copy_changed)
    install_dependency_keys(dependencies)
    print("Installed Workshop dependencies: " + ", ".join(dependencies), flush=True)


def install_dependency_keys(dependencies):
    keys = ROOT / "runtime/keys"
    keys.mkdir(parents=True, exist_ok=True)
    for name in dependencies:
        for key in (ROOT / "mods" / name).rglob("*.bikey"):
            copy_changed(key, keys / key.name)


def stage_realism_mods():
    if not release.LOCK.is_file():
        return
    locked = json.loads(release.LOCK.read_text())["dependencies"]
    if not release.REALISM_MODS.keys() <= locked.keys():
        return
    missing = {name: item for name, item in release.REALISM_MODS.items()
               if release.fingerprints(ROOT / "mods" / name) != locked[name]["files"]}
    stage_dependencies(missing, locked)
    install_dependency_keys(release.REALISM_MODS)


def runtime():
    require_closed_runtime()
    dest=ROOT/"runtime"
    dest.mkdir(exist_ok=True)
    for name in ("addons","dta","nasdara","sakhal","mpmissions"):
        target=INSTALL/name
        link=dest/name
        if target.exists() and not link.exists():
            run(["cmd","/c","mklink","/J",link,target])
    for path in INSTALL.iterdir():
        if path.is_file() and path.suffix.lower() in (".exe",".dll",".txt",".xml",".gproj"):
            out=dest/path.name
            if not out.exists() or out.stat().st_size != path.stat().st_size or out.stat().st_mtime_ns != path.stat().st_mtime_ns:
                shutil.copy2(path,out)
    (dest/"keys").mkdir(exist_ok=True)
    shutil.copytree(INSTALL/"battleye",dest/"battleye",dirs_exist_ok=True)
    for key in (INSTALL/"keys").glob("*.bikey"):
        shutil.copy2(key,dest/"keys"/key.name)
    stage_dependencies(release.DEPENDENCIES)


def mission():
    require_closed_runtime()
    import world
    for name in world.existing_profiles():
        world.select_profile(name)
        if world.running():
            raise RuntimeError(f"Stop the {name} server before rebuilding.")
        world.install_mission()
    world.select_profile("main")
    world.setup()


def stage_signed_addon(source, staging, authority):
    pbo = staging / (source.name + ".pbo")
    signature = pbo.with_name(pbo.name + "." + authority + ".bisign")
    run([TOOLS/"PboUtils/FileBank.exe", "-property", "prefix=" + source.name, "-dst", staging, source])
    if not pbo.is_file():
        raise RuntimeError("FileBank did not create a fresh " + source.name + " package.")
    run([TOOLS/"DsUtils/DSSignFile.exe", ROOT/"private-signing"/(authority+".biprivatekey"), pbo])
    if not signature.is_file():
        raise RuntimeError("DSSignFile did not create the package signature.")
    return pbo, signature


def pack_workshop_laser_companion():
    import workshop_lasers

    if not (WORKSHOP / workshop_lasers.ITEM_ID / "addons/MutatedLasers.pbo").is_file():
        return
    require_closed_runtime()
    mod = ROOT / "mods" / workshop_lasers.COMPANION_FOLDER
    addons, keys = mod / "addons", mod / "keys"
    addons.mkdir(parents=True, exist_ok=True)
    keys.mkdir(exist_ok=True)
    installed = {name: release.digest(mod / name) for name in workshop_lasers.COMPANION_PACKAGE if (mod / name).is_file()}
    record = workshop_lasers.companion_build_record(ROOT)
    previous = record.get("previous", [])
    if len(installed) == len(workshop_lasers.COMPANION_PACKAGE) and installed not in previous:
        previous.append(installed)
    with TemporaryDirectory(prefix=".pack-", dir=mod) as temporary:
        pbo, signature = stage_signed_addon(ROOT / "source/compat/PredateMutatedLasers", Path(temporary), "PredateLocal")
        current = {"addons/" + path.name: release.digest(path) for path in (pbo, signature)}
        require_closed_runtime()
        os.replace(signature, addons / signature.name)
        os.replace(pbo, addons / pbo.name)
    copy_changed(ROOT / "private-signing/PredateLocal.bikey", keys / "PredateLocal.bikey")
    release.write_changed(ROOT / workshop_lasers.COMPANION_BUILD_RECORD, json.dumps({
        "schema": 1, "current": current, "previous": [item for item in previous if item != current],
    }, indent=2, sort_keys=True) + "\n")
    print("Built and signed the Workshop laser compatibility addon.", flush=True)


def pack(candidate=False):
    require_closed_runtime()
    layouts()
    mod_root=ROOT/"mods"/("@PredateCandidate" if candidate else "@Predate")
    dest=mod_root/"addons"
    dest.mkdir(parents=True,exist_ok=True)
    release.write_changed(mod_root/"mod.cpp",'name="PREDATE"; author="PREDATE contributors"; version="0.3.0";')
    manifest = ROOT / "build" / ("candidate-build-manifest.json" if candidate else "build-manifest.json")
    inputs = release.source_fingerprint()
    modern_loadouts = "launcher/resources/modern-loadouts.json"
    inputs[modern_loadouts] = release.digest(ROOT / modern_loadouts)
    inputs["config/release-lock.json"] = release.digest(release.LOCK)
    for name in ("build.py", "release.py", "world.py"):
        path = f"scripts/{name}"
        inputs[path] = release.digest(ROOT / path)
    pbo = dest / "Predate.pbo"
    signature = dest / "Predate.pbo.PredateLocal.bisign"
    precision_sources = {
        "PredateStockPrecision": dest,
        "PredateBucaPrecision": mod_root / "buca-precision" / "addons",
    }
    precision_files = [
        folder / (name + suffix)
        for name, folder in precision_sources.items()
        for suffix in (".pbo", ".pbo.PredateLocal.bisign")
    ]

    def precision_digests():
        return {
            path.relative_to(mod_root).as_posix(): release.digest(path)
            for path in precision_files if path.is_file()
        }

    if manifest.exists() and pbo.exists() and signature.exists():
        previous = json.loads(manifest.read_text())
        if (previous.get("inputs") == inputs
                and previous.get("pbo_sha256") == release.digest(pbo)
                and previous.get("signature_sha256") == release.digest(signature)
                and all(path.is_file() for path in precision_files)
                and previous.get("precision_sha256") == precision_digests()):
            print("PREDATE package already matches these release inputs.", flush=True)
            return
    private=ROOT/"private-signing"
    private.mkdir(exist_ok=True)
    authority="PredateLocal"
    if not (private/(authority+".biprivatekey")).exists():
        run([TOOLS/"DsUtils/DSCreateKey.exe",authority],cwd=private)
    # FileBank can exit successfully without replacing an existing package.
    # Build the package and signature from scratch before touching the live pair.
    with TemporaryDirectory(prefix=".pack-", dir=mod_root.resolve()) as staging:
        staging = Path(staging)
        source = staging / "Predate"
        shutil.copytree(ROOT / "source/Predate", source)
        bundled_loadouts = source / "data/modern-loadouts.json"
        shutil.copy2(ROOT / modern_loadouts, bundled_loadouts)
        if release.digest(bundled_loadouts) != inputs[modern_loadouts]:
            raise RuntimeError("Modern loadouts changed during packaging. Retry the build.")
        staged_pbo, staged_signature = stage_signed_addon(source, staging, authority)
        packages = [(staged_signature, signature), (staged_pbo, pbo)]
        for name, folder in precision_sources.items():
            addon = ROOT / "source" / "compat" / name
            if (addon / "config.cpp").is_file():
                deployed_source = staging / name
                shutil.copytree(addon, deployed_source)
                aliases = []
                for ammo in ROUND_AMMO[name]:
                    if name != "PredateStockPrecision" or ammo not in (
                        "Bullet_12GaugeSlug", "Bullet_12GaugeBeanbag", "Bullet_12GaugePellets",
                    ):
                        aliases.append(f"class {ammo};\n")
                    for slot in range(ROUND_SLOTS):
                        aliases.append(f"class PR_{slot}_{ammo}: {ammo} {{}};\n")
                (deployed_source / "predate-rounds.hpp").write_text("".join(aliases), encoding="ascii")
                addon = deployed_source
            addon_pbo, addon_signature = stage_signed_addon(addon, staging, authority)
            packages.extend((path, folder / path.name) for path in (addon_signature, addon_pbo))
        require_closed_runtime()
        previous_files = {}
        for index, (_, target) in enumerate(packages):
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.is_file():
                previous_file = staging / f"previous-{index}"
                shutil.copy2(target, previous_file)
                previous_files[target] = previous_file
        published = []
        try:
            for staged, target in packages:
                os.replace(staged, target)
                published.append(target)
        except OSError:
            for target in reversed(published):
                if target in previous_files:
                    os.replace(previous_files[target], target)
                else:
                    target.unlink()
            raise
    for folder in [ROOT/"runtime/keys", mod_root/"keys"]:
        folder.mkdir(exist_ok=True)
        shutil.copy2(private/(authority+".bikey"),folder/(authority+".bikey"))
    print("Built and signed the PREDATE client/server mod.",flush=True)
    release.write_changed(manifest, json.dumps({"schema": 1, "inputs": inputs,
        "pbo_sha256": release.digest(pbo), "signature_sha256": release.digest(signature),
        "precision_sha256": precision_digests()}, indent=2, sort_keys=True) + "\n")


if __name__=='__main__':
    import world
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--accept-dependency-update", action="store_true",
                        help="Explicitly snapshot installed Workshop and DayZ versions")
    parser.add_argument("--candidate", action="store_true", help="Build a separate validation addon; retain the playable release and worlds")
    args = parser.parse_args()
    require_closed_runtime()
    for name in world.existing_profiles():
        world.select_profile(name)
        if world.running(): raise RuntimeError(f"Stop {name} before rebuilding.")
    world.select_profile("main")
    if args.accept_dependency_update:
        runtime()
        release.accept()
    else:
        stage_realism_mods()
        release.verify()
    if not args.candidate: mission()
    pack(candidate=args.candidate)
    if not args.candidate: pack_workshop_laser_companion()
    print("Build complete. Use scripts/Play PREDATE.cmd.",flush=True)
