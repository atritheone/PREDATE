PREDATE desktop launcher

TypeScript + React renderer, isolated preload API, Electron main process, and a
packaged Python session service. The app works locally and loads no remote UI.

Development:
  npm ci
  npm run dev

Checks and production build:
  npm run format:check
  npm run typecheck
  npm run package

The package command creates the Python build environment, bundles the backend,
builds the TypeScript app, and produces release/PREDATE.exe for Windows x64.
After packaging, close the launcher and copy release/PREDATE.exe to the project
root to update the executable used by the existing CMD shortcuts.
The packaged launcher requires no separately installed Python or Node runtime.
DayZ, Steam, the server, mod files and saved worlds remain in the PREDATE project.

New Operation puts Game, Spawn, and Loadout side by side in the expanded window.
Create and Play applies the selected clock, enemies, respawns, spawn location,
and starting inventory to a new world. Save Draft stores the
setup on this PC for later. Random region is the default spawn choice.

The Chernarus relief and item catalogue come from the installed game. To
regenerate them, run source/validation/operation_catalog.c as init.c in a
separate Chernarus mission, then pass that server's profile directory to
launcher-app/scripts/operation_assets.py.

Architecture:
  src/shared       Typed API and data contracts
  src/preload      Narrow interface exposed to the renderer
  src/main         App lifecycle, project discovery, backend process and IPC
  src/renderer     Continue, New Operation, Settings and Backups
  scripts          Reproducible backend and app-icon build
  ../scripts/backend_entry.py  JSON bridge and packaged helper entry points
  ../scripts/session.py        Session lifecycle supervision
  ../scripts/session_store.py  Preferences, worlds and verified save snapshots

The session supervisor and crash watchdogs run independently of the launcher UI.
Closing the launcher leaves an active game running. Save and Quit closes the game
normally, saves and stops the server, and creates a verified backup.

Steam Offline Mode failed the local connection test on this DayZ build. Normal
play currently requires Steam to be signed in online. The launcher bundles its
UI assets.

Backend executables are cached by hash in session-runtime/backend so portable-app
temporary-folder cleanup cannot interrupt an active game session.
