"""Map identity shared by mission setup, operation storage and the launcher."""
from dataclasses import dataclass


DEFAULT_MAP = "chernarusplus"


@dataclass(frozen=True)
class Terrain:
    id: str
    name: str
    size: int
    base_manifest: str
    start_date: str = "09/20"
    dlc: str | None = None

    @property
    def mission_lock(self):
        if self.id == DEFAULT_MAP:
            return "mission_base"
        if self.id == "enoch":
            return "livonia_mission_base"
        return self.id + "_mission_base"

    @property
    def mission(self):
        return f"solo.{self.id}"

    @property
    def native_mission(self):
        return f"dayzOffline.{self.id}"


TERRAINS = {
    DEFAULT_MAP: Terrain(DEFAULT_MAP, "Chernarus", 15360, "base-files.json"),
    "enoch": Terrain("enoch", "Livonia", 12800, "base-files.enoch.json"),
    "sakhal": Terrain("sakhal", "Sakhal", 15360, "base-files.sakhal.json", "02/01", "Frostline"),
    "nasdara": Terrain("nasdara", "Nasdara", 16384, "base-files.nasdara.json", "04/30", "Badlands"),
}


def terrain(value=DEFAULT_MAP):
    if not isinstance(value, str) or value not in TERRAINS:
        raise ValueError("Choose a supported hunt map.")
    return TERRAINS[value]


def operation_terrain(metadata):
    return terrain(metadata.get("map", DEFAULT_MAP))
