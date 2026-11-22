# PREDATE agent instructions

When a player asks to set up PREDATE, perform the setup in this checkout and verify the result. Use the repository's existing build and launcher scripts. Carry out the steps available on the player's machine and ask them for specific actions that require their Steam account or desktop interaction. Continue the setup after those steps are complete.

## Check prerequisites

- PREDATE runs on Windows x64. Check for `py -3`, Node.js 24, and npm. Use `npm.cmd` in PowerShell if the npm script shim is blocked. Reuse installed tools that meet the build requirements.
- Check for the DayZ client, DayZ Server, and DayZ Tools. The build and session tools in `scripts/` resolve game inputs under `C:\Program Files (x86)\Steam\steamapps`. If Steam uses another library, identify the actual paths and resolve the path mismatch before building.
- Check that Steam has finished downloading the Workshop dependencies in `workshop/content/221100`. `scripts/build.py` checks for `Addons/*.pbo` in each Workshop item.

| Dependency | Workshop item ID |
| --- | --- |
| CF | `1559212036` |
| Dabs Framework | `2545327648` |
| DayZ Expansion Core | `2291785308` |
| DayZ Expansion AI | `2792982069` |
| Better-Snap-Sounds | `2565724570` |
| Reactive Ragdolls | `3824364587` |

Normal play requires Steam to be signed in online. If Steam needs user interaction, report the exact missing game, tool, or Workshop item so the player can resolve it. If a PREDATE session is running, save and quit through the launcher or game menu before building. Close other DayZ clients or servers normally before replacing the runtime or addon.

## Build the mod and launcher

1. From the repository root, provision the local runtime and Workshop copies on a fresh checkout.

   ```powershell
   Push-Location scripts
   py -3 -c "import build; build.runtime()"
   Pop-Location
   ```

   Reuse a provisioned `runtime/` and `mods/` on an existing installation. Full staging synchronizes all six managed dependency folders to their installed Workshop copies, so inspect any existing staged dependencies before refreshing them. The normal build stages or repairs the Better-Snap-Sounds and Reactive Ragdolls folders when needed and the Steam downloads match the release lock.

2. Build and sign the addon against the checked-in release lock.

   ```powershell
   py -3 scripts/build.py
   ```

   If verification fails, identify the missing or mismatched inputs and use matching versions where available. Treat `--accept-dependency-update` as a dependency upgrade. It restages Workshop files and rewrites `config/release-lock.json`. Native mission inputs also need to match `source/mission/base-files.json`.

3. From `launcher/`, install the locked dependencies and build the Windows portable launcher.

   ```powershell
   npm.cmd ci
   npm.cmd run package
   ```

   After packaging succeeds, close any running PREDATE launcher and copy `launcher/release/PREDATE.exe` to the repository root.

## Verify setup

- Confirm `runtime/DayZServer_x64.exe`, `mods/@Predate/addons/Predate.pbo`, and the root `PREDATE.exe` exist. Run `py -3 scripts/world.py status --profile main` from the repository root to check the local profile.
- If the desktop is available, open `PREDATE.exe` from the root and confirm that it reaches the launcher menu. Leave operation selection to the player. Point them to **New Operation** for a new game or **Continue** for an existing save.
- Report which checks passed, whether an in-game connection was verified, and any remaining blocker.

## Preserve local state

- Preserve existing saves, backups, and local preferences in `world/`, `worlds/`, `backups/`, and `config/*.local.json`. If upgrading an older checkout, save and quit with the existing launcher before moving the root `settings.local.json`, `launcher-settings.local.json`, and `active-release.local.json` into `config/`. Check for destination conflicts before moving files. If the previous session supervisor remains running, verify its process identity and confirm the session is suspended with no active session or queued requests before retiring it. The rebuilt launcher starts the new supervisor when needed.
- Keep local worlds, credentials, signing keys, staged dependencies, and generated packages out of Git. Follow `.gitignore` and keep `predate.txt` eligible for version control.
- Run `npm.cmd run build` from `launcher/` for launcher changes.
