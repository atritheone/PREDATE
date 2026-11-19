<p align="center">
  <img src="flash/predate_logo.png" alt="PREDATE logo" width="300" />
</p>

<h1 align="center">PREDATE</h1>

<p align="center">
  A persistent single-player DayZ sandbox where you and rival AI operators hunt one another across Chernarus.
</p>

<p align="center">
  <a href="https://atritheone.com/predate">Website</a>
  ·
  <a href="#how-an-operation-works">How to play</a>
  ·
  <a href="#quick-start">Quick start</a>
  ·
  <a href="#building-and-packaging">Builds</a>
  ·
  <a href="#testing">Testing</a>
</p>

## About the game

Pick an insertion point, choose your starting kit, and enter Chernarus alongside a roster of AI rivals. Your receiver gives you intermittent bearing estimates. Move between pulses to narrow the search, then rely on sight and sound as you close in. Rival operators work from their own observations and signals, so the player can become the target while tracking someone else.

The operation persists between sessions. Surviving operators carry their injuries and remaining ammunition into the next encounter, and you can redeploy into the same world after death. PREDATE keeps DayZ's movement, stamina, and gunplay while disabling infected, hunger, thirst, and the world loot economy.

## Highlights

- First-person hunts across the full Chernarus map with a configurable roster of up to 30 AI rivals.
- Scheduled bearing pulses on the operation map and compass, with interception warnings when a rival is tracking you.
- Custom insertion points, loadouts, time of day, and respawn settings for each operation.
- Persistent saves, redeployment, and backup restoration through the Windows launcher.

## How an operation works

1. Choose **New Operation** and set the game rules, spawn, and loadout. Select **Create and Play** to deploy.
2. Open the operation map to see bearing pulses from rivals within 10 km of you when sampled. Select an arc to track that signal on your compass.
3. Use successive samples from different positions to narrow the search. The compass hands off to sight and sound inside 200 metres.
4. Use **Save and Quit** to save the operation and stop the local server. Select **Continue** in the launcher to resume it.

If you die, choose **Redeploy** to return with the operation's starting kit or save from the debrief and return later.

## Controls

| Input | Action |
| --- | --- |
| <kbd>M</kbd> | Open the operation map. Select a bearing arc or name to track a rival. |
| <kbd>Esc</kbd> | Pause the operation and open the game menu. |
| Tap <kbd>N</kbd> | Toggle equipped night vision goggles. |
| Tap <kbd>R</kbd> | Reload the equipped firearm. |
| Double-tap <kbd>R</kbd> | Rack the action or clear a jam. |
| Hold <kbd>R</kbd> | Check roughly how full the magazine is. |

## Quick start

PREDATE is set up with a coding agent on your Windows PC. Download or clone this repository, open its folder in your coding agent, and ask it to run the setup.

> Set up PREDATE on this PC. Read `AGENTS.md`, check my Steam and build dependencies, and get the game launcher ready to play.

The agent follows [AGENTS.md](AGENTS.md) to check your installation, build the DayZ mod, package `PREDATE.exe`, and verify the launcher. If Steam needs you to sign in or install missing game content, the agent will tell you what to do and continue once it is ready.

Once setup is complete, keep Steam signed in online and open `PREDATE.exe` or `scripts/Play PREDATE.cmd`. Choose **New Operation** to start fresh or **Continue** to resume an existing save. The portable launcher bundles its Python backend.

## Building and packaging

These commands are a reference for coding agents and contributors. For a guided installation, use the [quick start](#quick-start).

### DayZ mod

Install DayZ, DayZ Server, and DayZ Tools in the default Steam library. Subscribe to CF, Dabs Framework, DayZ Expansion Core, and DayZ Expansion AI in the Steam Workshop. Install Python 3 with the Windows `py` launcher and close DayZ and its server before building.

From the repository root, populate the local runtime and mod copies on the first build, then build against the checked-in release lock.

```powershell
Push-Location scripts
py -3 -c "import build; build.runtime()"
Pop-Location
py -3 scripts/build.py
```

The build checks installed inputs against [release-lock.json](config/release-lock.json). If Steam has updated a dependency, review the change before refreshing the lock with `py -3 scripts/build.py --accept-dependency-update`.

### Windows launcher

With Node.js 24, npm, and Python 3 installed, run the following from the repository root.

```powershell
Set-Location launcher-app
npm ci
npm run package
```

Copy `launcher-app/release/PREDATE.exe` to the repository root for the CMD shortcuts. During launcher development, run `npm run dev` from `launcher-app/`.

## Project structure

| Path | Contents |
| --- | --- |
| [`source/Predate/`](source/Predate/) | DayZ mod scripts, configurations, and in-game UI assets. |
| [`source/mission/`](source/mission/) | Chernarus mission setup and configuration. |
| [`source/validation/`](source/validation/) | Native DayZ validation scenarios and game-data exports. |
| [`launcher-app/`](launcher-app/) | Electron launcher, React UI, and Windows packaging scripts. |
| [`scripts/`](scripts/) | Build tools, launcher shortcuts, local server controls, and Python session handling. |
| [`config/`](config/) | Release lock and local JSON settings. |
| [`flash/`](flash/) | PREDATE branding and source artwork. |
| [`tests/`](tests/) | Python unit tests and native DayZ integration checks. |

## Testing

Run the Python unit tests from the repository root and build the launcher with its installed dependencies.

```powershell
py -3 -m unittest discover -s tests -v
Set-Location launcher-app
npm run build
```

The native integration checks require a provisioned DayZ installation and use dedicated development or test worlds.
