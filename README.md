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

Choose a terrain and insertion point, then enter the map alongside a roster of AI Operators. Move between pulses to narrow the search, then rely on sight and sound as you close in. Every Operator has a private receiver with the same pulse rules. AI Operators pursue the closest contact they can infer from their own readings and observations, and move on when a hunt stalls.

The hunt persists between sessions. Surviving Operators carry their injuries and remaining ammunition into the next encounter, and you can redeploy into the same world after death.

## Highlights

- First-person hunts on Chernarus, Livonia, Sakhal, or Nasdara with 1–100 AI Operators. New hunts default to 100.
- Wildlife and infected spawns are disabled. Animals cannot damage the player or Operators.
- Operators carry rifles ready on patrol. Stamina never drains for the player or Operators, and bullet damage between them is doubled.
- Weapons, suppressors, and wearable gear keep their condition for the player and Operators. Ammunition and single-use items are still consumed.
- Scheduled bearing pulses on the map and compass, with one interception warning per continuous hunt.
- Custom insertion points, loadouts, time of day, and respawn settings for each hunt.
- Persistent saves, redeployment, and backup restoration through the launcher.

## How a hunt works

1. Choose **New Hunt** and set the game rules, spawn, and loadout. Select **Create and Play** to deploy.
2. Open the map to see bearing pulses from Operators within 5 km of you when sampled. Each pulse covers a 15° range that can shift between samples. Overlapping arcs stack with nearer Operators inside farther ones. Every pulse has a clickable nameplate on the map. When a group is too dense for all its connecting lines to stay visible, hover over a name to show its line and highlight its arc. Hovering also previews the Operator and sampled distance. Pulse colours run from yellow for distant signals to red for nearby ones. The tracked arc is white with a distance-coloured outline, and the compass readout includes the estimated distance band.
3. Select a contact to see its historical sectors on the terrain from where you took each reading. Distance estimates use 200-metre bands. Final Approach starts within 200 metres of your tracked Operator. The compass keeps their callsign visible while you track by sight and sound, and earlier sectors stay on the map. Bearing pulses still run every 90 seconds. Leaving Final Approach keeps your target selected and brings back its last valid reading. Once that reading expires, the map labels the most recent historical sector as Last Known.

Nearby footprints can be read with a brief look. The combat log reports a foreign trail once, with a direction when the marks are clear. Your own prints are visible and labelled on inspection. AI Operators can read and investigate the same clues. Footprint material adapted from [ambientCG Footsteps 002](https://ambientcg.com/view?id=Footsteps002), CC0.

AI Operators get the same Final Approach cue for their own tracked contact. They pause to look and listen, then walk a short search guided by remembered bearings and anything they see or hear. They keep their normal combat and recovery priorities.
4. Use **Save and Quit** to save the hunt and stop the local server. Select **Continue** in the launcher to resume it.

Choose Chernarus, Livonia, Sakhal, or Nasdara under **Map** in **New Hunt**, then select a named insertion, let PREDATE pick a random region, or choose exact coordinates on the map. Sakhal requires Frostline and Nasdara requires Badlands on the current Steam account. The launcher greys out DLC maps with missing local files. DayZ checks ownership when you launch a hunt that uses a DLC map. The map is recorded with the hunt, and Continue and Restore Copy use that terrain. Earlier hunts and backups without a map field belong to Chernarus.

Sakhal hunts start on the main island, with surface insertions for players and Operators. Nasdara spreads surface insertions across the desert, foothills, and settlement outskirts. Both maps use PREDATE's clear weather and disabled survival, wildlife, infected, vehicles, and ambient loot rules.

Each new hunt shuffles its Operator starting positions. At least one Operator starts 1.5–3.2 km from your insertion, while larger rosters fill regions across the selected map. Dense starting rosters allow Operators to start closer to each other. The 1.5 km separation for redeployment and Operator respawns still applies.

The pause menu's **Suicide** option asks for confirmation before ending your current life. On death, the screen immediately shows **Death** on solid red. **Redeploy** and **Save and Quit** appear two seconds later on the same screen. Redeploy returns you with the hunt's starting kit. PREDATE searches for safe ground at least 1.5 km from every living Operator, with multiple contacts within the 5 km receiver range when possible. It favors areas away from recent insertions and at least 5 km from where you died, with a 3.5 km fallback and closer sites available if needed. Operator and player corpses cannot be looted.

Pick player and AI equipment separately from the weapon preset in **New Hunt**. **Spectral** is the player default, with a white T-shirt, beige canvas trousers, brown work boots, a tan plate carrier, and black gloves and night vision. **Ghost** uses a black carrier. **Operator** pairs Spectral's gear with blue jeans and black sneakers. **File** is the original camouflage uniform. The presets mount NVGs on a head strap and carry supplies in clothing and vest pouches. AI Operators have matching black or tan equipment and eye-opening balaclavas.

The modern rifle presets include the **HK417** (the default), HK416D, M4A1, G36K, MDR, AK-47 Tactical, SCAR-H, SA-58, G28, CZ TSR, M110, and R11 RSASS. These presets include a scope, a suppressor, a weapon module with visible and infrared light and laser modes, and powered GPNVG-18 goggles. They use two shared Workshop packs. See [Steam Workshop dependencies](#steam-workshop-dependencies).

With both packs loaded, AI Operators start with a mix of modern rifle presets. Each new deployment moves that Operator to the next rifle. Continuing a hunt restores the rifles already carried by its living Operators.

## Controls

| Input | Action |
| --- | --- |
| <kbd>M</kbd> | Open or close the map. Select a bearing arc or name to track an Operator. |
| <kbd>Esc</kbd> | Close the map, or pause and open the game menu during play. |
| <kbd>F</kbd> | Interact. An equipped firearm lowers briefly as needed. |
| <kbd>1</kbd>–<kbd>4</kbd> | Use a hotbar slot. The default rifle and sidearm are in slots 1 and 2. |
| Scroll down | Lower a held firearm to low ready. |
| Scroll up | Return a held firearm to PREDATE's default raised stance. |
| Tap <kbd>N</kbd> | Toggle equipped night vision goggles. |
| <kbd>5</kbd> | Toggle the white weapon flashlight. |
| <kbd>6</kbd> | Toggle the selected laser on or off. |
| <kbd>7</kbd> | Switch the selected laser between visible and IR. An active laser stays on in the new mode. |
| <kbd>8</kbd> | Toggle the IR weapon illuminator. |
| Tap <kbd>R</kbd> | Reload the equipped firearm. |
| Double-tap <kbd>R</kbd> | Rack the action or clear a jam. |
| Hold <kbd>R</kbd> | Check roughly how full the magazine is. |

Low ready uses DayZ's lowered stance and standard right-mouse aiming. While aiming down sights, the wheel keeps its normal optics controls.

Head-mounted NVGs blur the view through standard weapon scopes. Tap <kbd>N</kbd> to turn the goggles off and clear the view without lowering the weapon.

With active goggles, entering a night vision scope plays the goggles-off cue. Leaving the scope plays the goggles-on cue if the goggles are still active.

Keys <kbd>5</kbd> through <kbd>8</kbd> control the supported light and laser modes on the weapon in your hands, including sidearms. A white-light-only attachment uses <kbd>5</kbd>. Mode and power changes appear in the same bottom-left HUD log as fire mode and zeroing. The IR laser and illuminator require active night vision. The original PREDATE module also supports <kbd>K</kbd> to select a mode and <kbd>U</kbd> to toggle it.

Weapon lights and lasers stop emitting when holstered. Drawing the weapon restores the settings you left on that weapon.

The default rifle kits use the Workshop module when installed. AI Operators holding a rifle use its visible laser in daylight. In low light they lower their quad goggles and use the IR laser and illuminator while the goggles are powered.

Operators can investigate a visible laser beam, its impact point, or a flashlight splash from another Operator. Observing IR cues requires active night vision. A white beam at night or an IR illuminator viewed through night vision can help an Operator spot someone actually lit by the beam. The observer still needs a clear view, and a light clue gives them an area to search.

## Installation

PREDATE requires 64-bit Windows 10 or Windows 11. Download or clone this repository, open its folder in your coding agent, and ask it to run the setup.

> Set up PREDATE on this PC. Read `AGENTS.md`, check my Steam and build dependencies, and get the game launcher ready to play.

The agent follows [AGENTS.md](AGENTS.md) to check your installation, build the DayZ mod, package `PREDATE.exe`, and verify the launcher. If Steam needs you to sign in or install missing game content, the agent will tell you what to do and continue once it is ready.

### Steam Workshop dependencies

Subscribe to these six packs in Steam and let their downloads finish. PREDATE setup stages the locked versions locally and loads them on both the client and server.

| Workshop pack | Item ID |
| --- | --- |
| [CF](https://steamcommunity.com/sharedfiles/filedetails/?id=1559212036) | `1559212036` |
| [Dabs Framework](https://steamcommunity.com/sharedfiles/filedetails/?id=2545327648) | `2545327648` |
| [DayZ Expansion Core](https://steamcommunity.com/sharedfiles/filedetails/?id=2291785308) | `2291785308` |
| [DayZ Expansion AI](https://steamcommunity.com/sharedfiles/filedetails/?id=2792982069) | `2792982069` |
| [Better-Snap-Sounds](https://steamcommunity.com/sharedfiles/filedetails/?id=2565724570) | `2565724570` |
| [Reactive Ragdolls](https://steamcommunity.com/sharedfiles/filedetails/?id=3824364587) | `3824364587` |

Better-Snap-Sounds changes bullet flyby audio. Reactive Ragdolls adds physics-driven falls for players and supported AI Operators. Both apply to new and resumed hunts. If you previously added either pack through **Extra Mods**, setup uses the staged copy without loading it twice.

To upgrade an existing installation, use **Save and Quit** if a hunt is active, then rerun setup after Steam finishes downloading both packs.

New hunts also require both gear packs for the modern AI rifle roster, even if the player chooses a custom inventory. The launcher adds the packs automatically after Steam finishes downloading them. Existing hunts retain their saved gear and other optional mods.

| Workshop pack | Item ID | Equipment |
| --- | --- | --- |
| [Buca Weapons](https://steamcommunity.com/sharedfiles/filedetails/?id=3092251597) | `3092251597` | Rifles, magazines, and weapon attachments |
| [Forward Operator Gear](https://steamcommunity.com/sharedfiles/filedetails/?id=2931560672) | `2931560672` | GPNVG-18 goggles |

The AN/PEQ-15 Laser / Light Module comes from [MutatedMods Lasers](https://steamcommunity.com/sharedfiles/filedetails/?id=3704871552) (item `3704871552`) and requires PREDATE's signed local compatibility addon. The companion supplies the controls above and disables the Workshop pack's automatic Discord usage report. PREDATE verifies the pair before starting a hunt.

### Developer server diagnostics

Launch `PREDATE.exe --server-diagnostics`, then start or continue a hunt to capture server FPS, memory, checkpoint timings, and console and admin logs. The flag applies the next time a server starts. Logs are under the operation's `profile` directory. For per-Operator route and perception traces, add `PredateAITrace.txt` to that profile before starting the server. Regular play keeps DayZ's built-in diagnostics available for recovery and leaves Expansion tracing off.

Once setup is complete, keep Steam signed in online and open `PREDATE.exe` or `scripts/Play PREDATE.cmd`. Choose **New Hunt** to start fresh or **Continue** to resume an existing save. PREDATE uses the official `DayZDiag_x64.exe` from your Steam installation for both the client and its localhost server, with BattlEye off. The portable launcher bundles its Python backend.
