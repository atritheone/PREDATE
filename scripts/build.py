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
WORKSHOP = Path(r"C:\Program Files (x86)\Steam\steamapps\workshop\content\221100")
DEPENDENCIES = {"@CF":"1559212036", "@DabsFramework":"2545327648", "@ExpansionCore":"2291785308", "@ExpansionAI":"2792982069"}


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


def layouts():
    gui = ROOT / "source/Predate/gui"
    gui.mkdir(exist_ok=True)
    canvas = widget("CanvasWidgetClass", "MapOverlay", 0,0,1,1,"ignorepointer 1\npriority 100")
    bearing_labels = "".join(map_label(f"MapBearing{index}", "0.70 0.85 0.95 1") for index in range(24))
    pulse_labels = "".join(map_label(f"MapPulse{index}", "1 0.79 0.44 1") for index in range(30))
    children = widget("MapWidgetClass", "Terrain", .016,.085,.644,.89,
                      'clipchildren 1', canvas+bearing_labels+pulse_labels)
    children += label("Title", "Operation Map", .016,.012,.32,.06,"0.82 0.91 0.96 1")
    children += widget("ButtonWidgetClass","Overview",.259,.023,.11,.044,
                       'style Default\ntext "Overview"')
    children += widget("ButtonWidgetClass","CenterPlayer",.379,.023,.123,.044,
                       'style Default\ntext "Center on me"')
    readout = label("TrackingTitle", "Tracking", .06,.018,.88,.053,"0.55 0.85 0.90 1")
    readout += label("Status", "Waiting for pulses", .06,.084,.88,.047)
    readout += label("Bearing", "No pulse", .06,.145,.88,.042)
    readout += label("Timing", "", .06,.20,.88,.04)
    readout += label("Hint", "", .06,.26,.88,.04)
    readout += label("InterceptionStatus", "You are being hunted.", .06,.31,.88,.045,
                     "1 0.698 0.325 1").replace('ignorepointer 1','ignorepointer 1\nvisible 0')
    readout += label("HistoryTitle", "Historical sectors", .06,.50,.88,.048,
                     "0.55 0.85 0.90 1").replace('ignorepointer 1','ignorepointer 1\nvisible 0')
    for index in range(4):
        row = label(f"History{index}", "", .06,.548 + index * .044,.88,.043)
        readout += row.replace('ignorepointer 1','ignorepointer 1\nvisible 0')
    readout = readout.replace('font "gui/fonts/etelkatextpro22"',
                              'scaled 0\nfont "gui/fonts/sdf_MetronBook24"\n"exact text" 1\n"exact text size" 15')
    readout += widget("ButtonWidgetClass","Select",.06,.366,.88,.057,
                       'visible 0\nstyle Default\ntext "Clear target lock"')
    readout += widget("ButtonWidgetClass","NewHunt",.06,.913,.52,.058,
                       'visible 0\nstyle Default\ntext "Reset test range"')
    readout += widget("ButtonWidgetClass","Deploy",.06,.366,.42,.057,
                       'visible 0\nstyle Default\ntext "Start another hunt"')
    readout += widget("ButtonWidgetClass","ConfirmDeploy",.52,.366,.42,.057,
                       'visible 0\nstyle Default\ntext "Confirm hunt"')
    readout += widget("ButtonWidgetClass","Close",.64,.913,.30,.058,
                       'style Default\ntext "Close"')
    children += widget("PanelWidgetClass","Readout",.676,.025,.308,.95,
                       'color 0.05 0.085 0.11 1\nstyle UIDefaultPanel',readout)
    panel = widget("PanelWidgetClass","Panel",.025,.035,.95,.93,
                   'color 0.025 0.045 0.06 1\nstyle UIDefaultPanel',children)
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
                    'ignorepointer 1\ncolor 0.02 0.03 0.04 0.40\nstyle UIDefaultPanel')
    border_style = 'ignorepointer 1\ncolor 0.95 0.16 0.20 1\nstyle rover_sim_colorable'
    border = ""
    for name, x, y, width, height in (
        ("Top", 0, 0, 1, .023), ("Bottom", 0, .747, 1, .023),
        ("Left", 0, .023, .0028, .724), ("Right", .9972, .023, .0028, .724),
    ):
        border += widget("PanelWidgetClass", f"Outline{name}", x,y,width,height, border_style)
    outline = widget("FrameWidgetClass", "FinalApproachOutline", 0,0,1,1,
                     'ignorepointer 1\nvisible 0\npriority 10', border)
    compass_parts=backdrop+heading+tape+notch+receiver+outline
    compass_parts=compass_parts.replace('font "gui/fonts/etelkatextpro22"', 'font "gui/fonts/etelkatextpro22"\n"exact text" 1\n"exact text size" 11')
    compass_parts=compass_parts.replace('font "gui/fonts/etelkatextpro22"', 'scaled 0\nfont "gui/fonts/sdf_MetronBook24"')
    compass=widget("FrameWidgetClass","Compass",.36,.018,.28,.06,"ignorepointer 1",compass_parts)
    footprint = label("FootprintClue", "", .35,.77,.30,.03,"0.77 0.83 0.85 1").replace(
        'ignorepointer 1', 'ignorepointer 1\nvisible 0\n"text halign" center').replace(
        'font "gui/fonts/etelkatextpro22"', 'scaled 0\nfont "gui/fonts/sdf_MetronBook24"\n"exact text" 1\n"exact text size" 14')
    footprint += widget("ImageWidgetClass", "FootprintMark", 0,0,.01,.01,
                        'ignorepointer 1\nvisible 0\nmode blend')
    notices = ""
    for index in range(6):
        notices += label(f"HuntNotice{index}", "", 0,index/6,1,1/6,"1 1 1 1").replace(
            'ignorepointer 1', 'ignorepointer 1\nvisible 0\n"text halign" right').replace(
            'font "gui/fonts/etelkatextpro22"', 'scaled 0\nfont "gui/fonts/sdf_MetronBook24"\n"exact text" 1\n"exact text size" 16')
    feed = widget("FrameWidgetClass","HuntFeed",.675,.035,.30,.21,"ignorepointer 1",notices)
    release.write_changed(gui/"hunt_feed.layout",widget("FrameWidgetClass","PredateHuntFeed",0,0,1,1,"ignorepointer 1\nvisible 0\npriority 900",feed))
    release.write_changed(gui/"hud.layout",widget("FrameWidgetClass","PredateHud",0,0,1,1,"ignorepointer 1",compass+footprint))


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
    mods=ROOT/"mods"
    mods.mkdir(exist_ok=True)
    for name,item in DEPENDENCIES.items():
        src=WORKSHOP/item
        if not src.is_dir() or not list(src.glob("[Aa]ddons/*.pbo")):
            raise RuntimeError(f"Workshop dependency is not installed: {name} ({item})")
        # Stage an exact snapshot so files removed by an upstream update cannot linger.
        target = mods / name
        target.mkdir(exist_ok=True)
        source_files = {p.relative_to(src) for p in src.rglob("*") if p.is_file()}
        for existing in target.rglob("*"):
            if existing.is_file() and existing.relative_to(target) not in source_files:
                existing.unlink()
        shutil.copytree(src,target,dirs_exist_ok=True,copy_function=copy_changed)
        for key in src.rglob("*.bikey"):
            shutil.copy2(key,dest/"keys"/key.name)
    print("Installed local copies of all four Workshop dependencies.",flush=True)


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


def pack(candidate=False):
    require_closed_runtime()
    layouts()
    mod_root=ROOT/"mods"/("@PredateCandidate" if candidate else "@Predate")
    dest=mod_root/"addons"
    dest.mkdir(parents=True,exist_ok=True)
    release.write_changed(mod_root/"mod.cpp",'name="PREDATE"; author="PREDATE contributors"; version="0.3.0";')
    manifest = ROOT / "build" / ("candidate-build-manifest.json" if candidate else "build-manifest.json")
    inputs = release.source_fingerprint()
    inputs["config/release-lock.json"] = release.digest(release.LOCK)
    for name in ("build.py", "release.py", "world.py"):
        path = f"scripts/{name}"
        inputs[path] = release.digest(ROOT / path)
    pbo = dest / "Predate.pbo"
    signature = dest / "Predate.pbo.PredateLocal.bisign"
    if manifest.exists() and pbo.exists() and signature.exists():
        previous = json.loads(manifest.read_text())
        if previous.get("inputs") == inputs and previous.get("pbo_sha256") == release.digest(pbo) and previous.get("signature_sha256") == release.digest(signature):
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
        staged_pbo = staging / pbo.name
        staged_signature = staging / signature.name
        run([TOOLS/"PboUtils/FileBank.exe", "-property", "prefix=Predate", "-dst", staging, ROOT/"source/Predate"])
        if not staged_pbo.is_file():
            raise RuntimeError("FileBank did not create a fresh PREDATE package.")
        run([TOOLS/"DsUtils/DSSignFile.exe", private/(authority+".biprivatekey"), staged_pbo])
        if not staged_signature.is_file():
            raise RuntimeError("DSSignFile did not create the package signature.")
        require_closed_runtime()
        previous_signature = staging / "previous.bisign"
        if signature.is_file():
            shutil.copy2(signature, previous_signature)
        os.replace(staged_signature, signature)
        try:
            os.replace(staged_pbo, pbo)
        except OSError:
            if previous_signature.is_file():
                os.replace(previous_signature, signature)
            else:
                signature.unlink()
            raise
    for folder in [ROOT/"runtime/keys", mod_root/"keys"]:
        folder.mkdir(exist_ok=True)
        shutil.copy2(private/(authority+".bikey"),folder/(authority+".bikey"))
    print("Built and signed the PREDATE client/server mod.",flush=True)
    release.write_changed(manifest, json.dumps({"schema": 1, "inputs": inputs,
        "pbo_sha256": release.digest(pbo), "signature_sha256": release.digest(signature)}, indent=2, sort_keys=True) + "\n")


if __name__=='__main__':
    import world
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--accept-dependency-update", action="store_true",
                        help="Explicitly snapshot installed Workshop and DayZ versions")
    parser.add_argument("--candidate", action="store_true", help="Build a separate validation addon; retain the playable release and worlds")
    args = parser.parse_args()
    require_closed_runtime()
    active = subprocess.run(["tasklist", "/FI", "IMAGENAME eq DayZ_x64.exe", "/FO", "CSV", "/NH"],
                            capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    if '"DayZ_x64.exe"' in active.stdout:
        raise RuntimeError("Close DayZ before replacing its addon package.")
    for name in world.existing_profiles():
        world.select_profile(name)
        if world.running(): raise RuntimeError(f"Stop {name} before rebuilding.")
    world.select_profile("main")
    if args.accept_dependency_update:
        runtime()
        release.accept()
    else:
        release.verify()
    if not args.candidate: mission()
    pack(candidate=args.candidate)
    print("Build complete. Use scripts/Play PREDATE.cmd.",flush=True)
