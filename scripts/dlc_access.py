"""Local DLC map files. The launched DayZ client checks ownership before joining."""
from functools import lru_cache

import maps
import release
import session_store as store
import world


@lru_cache(maxsize=1)
def packages(lock_stamp):
    return store.read_json(release.LOCK, {}).get("terrain_packages", {})


def files_ready(terrain):
    checks = packages(release.LOCK.stat().st_mtime_ns).get(terrain.id, {})
    for side, base in (("client", world.CLIENT), ("server", world.SERVER)):
        files = checks.get(side)
        if not files or any(not (base / terrain.id / name).is_file() for name in files):
            return False
    return (world.SERVER / "mpmissions" / terrain.native_mission / "db/events.xml").is_file()


def availability():
    return {item.id: not item.dlc or files_ready(item) for item in maps.TERRAINS.values()}


def require_files(map_id):
    selected = maps.terrain(map_id)
    if selected.dlc and not files_ready(selected):
        raise ValueError(f"{selected.name} needs its {selected.dlc} map files. Finish the DayZ download in Steam and reopen PREDATE.")
