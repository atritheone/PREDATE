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
  <a href="#how-a-hunt-works">How to play</a>
  ·
  <a href="#installation">Installation</a>
</p>

## About the game

Pick an insertion point, choose your starting kit, and enter Chernarus alongside a roster of AI Operators. Move between pulses to narrow the search, then rely on sight and sound as you close in. Every Operator has a private receiver with the same pulse rules. AI Operators pursue the closest contact they can infer from their own readings and observations, and move on when a hunt stalls.

The hunt persists between sessions. Surviving Operators carry their injuries and remaining ammunition into the next encounter, and you can redeploy into the same world after death.

## Highlights

- First-person hunts across the full Chernarus map with a configurable roster of up to 30 AI enemies.
- Operators carry rifles ready on patrol. Stamina never drains for the player or Operators, and bullet damage between them is doubled.
- Weapons, suppressors, and wearable gear keep their condition for the player and Operators. Ammunition and single-use items are still consumed.
- Scheduled bearing pulses on the map and compass, with one interception warning per continuous hunt.
- Custom insertion points, loadouts, time of day, and respawn settings for each hunt.
- Persistent saves, redeployment, and backup restoration through the launcher.

## How a hunt works

1. Choose **New Hunt** and set the game rules, spawn, and loadout. Select **Create and Play** to deploy.
2. Open the map to see bearing pulses from Operators within 5 km of you when sampled. Each pulse covers a 15° range that can shift between samples. Overlapping arcs stack with nearer Operators inside farther ones. Pulse colours run from yellow for distant signals to red for nearby ones. Select an arc to see its estimated distance band and track it on your compass.
3. Select a contact to see its historical sectors on the terrain from where you took each reading. The receiver hands off to sight and sound when a scheduled pulse finds the target inside 200 metres. Earlier sectors stay on the map.
4. Use **Save and Quit** to save the hunt and stop the local server. Select **Continue** in the launcher to resume it.

If you die, choose **Redeploy** to return with the hunt's starting kit. PREDATE looks across the map for safe ground at least 1.5 km from every living Operator. It favors sites with more Operators within the 5 km receiver range, then shorter approaches to those Operators. Your death location breaks ties. Operator and player corpses cannot be looted. You can also save and quit from the death screen.

Pick player and AI equipment separately from the weapon preset in **New Hunt**. **Spectral** is the player default, with a white T-shirt, beige canvas trousers, brown work boots, a tan plate carrier, and black gloves and night vision. **Ghost** uses a black carrier. **Operator** pairs Spectral's gear with blue jeans and black sneakers. **File** is the original camouflage uniform. The presets mount NVGs on a head strap and carry supplies in clothing and vest pouches. AI Operators have matching black or tan equipment and eye-opening balaclavas.

The modern rifle presets include the **HK417** (the default), HK416D, M4A1, G36K, MDR, AK-47 Tactical, SCAR-H, SA-58, G28, CZ TSR, M110, and R11 RSASS. These presets include a scope, a suppressor, a weapon module with visible and infrared light and laser modes, and powered GPNVG-18 goggles. They use two shared Workshop packs. See [Steam Workshop dependencies](#steam-workshop-dependencies).

With both packs loaded, AI Operators start with a mix of modern rifle presets. Each new deployment moves that Operator to the next rifle. Continuing a hunt restores the rifles already carried by its living Operators.

## Controls

| Input | Action |
| --- | --- |
| <kbd>M</kbd> | Open or close the map. Select a bearing arc or name to track an Operator. |
| <kbd>Esc</kbd> | Close the map, or pause and open the game menu during play. |
| <kbd>F</kbd> | Interact. An equipped firearm lowers briefly as needed. |
| Tap <kbd>N</kbd> | Toggle equipped night vision goggles. |
| <kbd>5</kbd> | Toggle the white weapon flashlight. |
| <kbd>6</kbd> | Toggle the selected laser on or off. |
| <kbd>7</kbd> | Switch the selected laser between visible and IR. An active laser stays on in the new mode. |
| <kbd>8</kbd> | Toggle the IR weapon illuminator. |
| Tap <kbd>R</kbd> | Reload the equipped firearm. |
| Double-tap <kbd>R</kbd> | Rack the action or clear a jam. |
| Hold <kbd>R</kbd> | Check roughly how full the magazine is. |

The Workshop weapon module reports mode and power changes in the same bottom-left HUD log as fire mode and zeroing. It emits one laser at a time. The visible laser and white light work with unaided vision. The IR laser and illuminator require active night vision. While you hold a weapon fitted with this module, <kbd>5</kbd> through <kbd>8</kbd> control the module instead of those quickbar slots. Hunts using the original PREDATE module continue to use <kbd>K</kbd> to select a mode and <kbd>U</kbd> to toggle it.

The default rifle kits use the Workshop module when installed. AI Operators holding a rifle use its visible laser in daylight. In low light they lower their quad goggles and use the IR laser and illuminator while the goggles are powered.

## Installation

PREDATE is set up with a coding agent on your Windows PC. Download or clone this repository, open its folder in your coding agent, and ask it to run the setup.

> Set up PREDATE on this PC. Read `AGENTS.md`, check my Steam and build dependencies, and get the game launcher ready to play.

The agent follows [AGENTS.md](AGENTS.md) to check your installation, build the DayZ mod, package `PREDATE.exe`, and verify the launcher. If Steam needs you to sign in or install missing game content, the agent will tell you what to do and continue once it is ready.

### Steam Workshop dependencies

Subscribe to these four packs for every PREDATE installation.

| Workshop pack | Item ID |
| --- | --- |
| [CF](https://steamcommunity.com/sharedfiles/filedetails/?id=1559212036) | `1559212036` |
| [Dabs Framework](https://steamcommunity.com/sharedfiles/filedetails/?id=2545327648) | `2545327648` |
| [DayZ Expansion Core](https://steamcommunity.com/sharedfiles/filedetails/?id=2291785308) | `2291785308` |
| [DayZ Expansion AI](https://steamcommunity.com/sharedfiles/filedetails/?id=2792982069) | `2792982069` |

New hunts require both gear packs for the modern AI rifle roster, even if the player chooses a custom inventory. The launcher adds the packs automatically after Steam finishes downloading them. Existing hunts retain their saved mod lists.

| Workshop pack | Item ID | Equipment |
| --- | --- | --- |
| [Buca Weapons](https://steamcommunity.com/sharedfiles/filedetails/?id=3092251597) | `3092251597` | Rifles, magazines, and weapon attachments |
| [Forward Operator Gear](https://steamcommunity.com/sharedfiles/filedetails/?id=2931560672) | `2931560672` | GPNVG-18 goggles |

The AN/PEQ-15 Laser / Light Module comes from [MutatedMods Lasers](https://steamcommunity.com/sharedfiles/filedetails/?id=3704871552) (item `3704871552`) and requires PREDATE's signed local compatibility addon. The companion supplies the controls above and disables the Workshop pack's automatic Discord usage report. PREDATE verifies the pair before starting a hunt.

Once setup is complete, keep Steam signed in online and open `PREDATE.exe` or `scripts/Play PREDATE.cmd`. Choose **New Hunt** to start fresh or **Continue** to resume an existing save. The portable launcher bundles its Python backend.
