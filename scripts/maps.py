"""Map identity shared by mission setup, operation storage and the launcher."""
from dataclasses import dataclass


DEFAULT_MAP = "chernarusplus"


@dataclass(frozen=True)
class Terrain:
    id: str
    name: str
    size: int
    base_manifest: str

    @property
    def mission(self):
        return f"solo.{self.id}"

    @property
    def native_mission(self):
        return f"dayzOffline.{self.id}"


TERRAINS = {
    DEFAULT_MAP: Terrain(DEFAULT_MAP, "Chernarus", 15360, "base-files.json"),
    "enoch": Terrain("enoch", "Livonia", 12800, "base-files.enoch.json"),
}


def terrain(value=DEFAULT_MAP):
    if not isinstance(value, str) or value not in TERRAINS:
        raise ValueError("Choose a supported hunt map.")
    return TERRAINS[value]


def operation_terrain(metadata):
    return terrain(metadata.get("map", DEFAULT_MAP))
