<p align="center">
  <img src="flash/predate_logo.png" alt="PREDATE logo" width="300" />
</p>

<h1 align="center">PREDATE</h1>

<p align="center">
  A single-player DayZ mod where Operators hunt one another.
</p>

<p align="center">
  <a href="https://atritheone.com/predate">Website</a>
  ·
  <a href="#how-an-operation-works">How to play</a>
  ·
  <a href="#installation">Installation</a>
</p>

## About the game

Pick an insertion point, choose your starting kit, and enter Chernarus alongside a roster of AI Operators. Move between pulses to narrow the search, then rely on sight and sound as you close in. Every Operator has a private receiver with the same pulse rules. AI Operators pursue the closest contact they can infer from their own readings and observations, and move on when a hunt stalls.

The operation persists between sessions. Surviving Operators carry their injuries and remaining ammunition into the next encounter, and you can redeploy into the same world after death.

## Highlights

- First-person hunts across the full Chernarus map with a configurable roster of up to 30 AI enemies.
- Operators carry rifles ready on patrol. Stamina never drains for the player or Operators, and bullet damage between them is doubled.
- Scheduled bearing pulses on the operation map and compass, with one interception warning per continuous hunt.
- Custom insertion points, loadouts, time of day, and respawn settings for each operation.
- Persistent saves, redeployment, and backup restoration through the launcher.

## How an operation works

1. Choose **New Operation** and set the game rules, spawn, and loadout. Select **Create and Play** to deploy.
2. Open the operation map to see bearing pulses from Operators within 5 km of you when sampled. Each pulse covers a 15° range that can shift between samples. Overlapping arcs stack with nearer Operators inside farther ones. Pulse colours run from yellow for distant signals to red for nearby ones. Select an arc to see its estimated distance band and track it on your compass.
3. Select a contact to see its historical sectors on the terrain from where you took each reading. The receiver hands off to sight and sound when a scheduled pulse finds the target inside 200 metres. Earlier sectors stay on the map.
4. Use **Save and Quit** to save the operation and stop the local server. Select **Continue** in the launcher to resume it.

If you die, choose **Redeploy** to return with the operation's starting kit or save from the debrief and return later.

## Controls

| Input | Action |
| --- | --- |
| <kbd>M</kbd> | Open or close the operation map. Select a bearing arc or name to track an Operator. |
| <kbd>Esc</kbd> | Close the operation map, or pause and open the game menu during play. |
| <kbd>F</kbd> | Interact. An equipped firearm lowers briefly as needed. |
| Tap <kbd>N</kbd> | Toggle equipped night vision goggles. |
| Tap <kbd>R</kbd> | Reload the equipped firearm. |
| Double-tap <kbd>R</kbd> | Rack the action or clear a jam. |
| Hold <kbd>R</kbd> | Check roughly how full the magazine is. |

## Installation

PREDATE is set up with a coding agent on your Windows PC. Download or clone this repository, open its folder in your coding agent, and ask it to run the setup.

> Set up PREDATE on this PC. Read `AGENTS.md`, check my Steam and build dependencies, and get the game launcher ready to play.

The agent follows [AGENTS.md](AGENTS.md) to check your installation, build the DayZ mod, package `PREDATE.exe`, and verify the launcher. If Steam needs you to sign in or install missing game content, the agent will tell you what to do and continue once it is ready.

Once setup is complete, keep Steam signed in online and open `PREDATE.exe` or `scripts/Play PREDATE.cmd`. Choose **New Operation** to start fresh or **Continue** to resume an existing save. The portable launcher bundles its Python backend.
