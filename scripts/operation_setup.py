"""Validate the New Operation payload before a world is created."""
from functools import cache
import json
from pathlib import Path
import sys

import loadouts
import maps

MAX_INVENTORY_ITEMS = 256
MAX_INVENTORY_DEPTH = 12
PLAYER_SLOTS = {
    "headgear", "mask", "eyewear", "body", "vest", "back", "legs", "feet",
    "gloves", "hips", "shoulder", "melee", "armband",
}
ENTRY_KEYS = {"ClassName", "Parent", "Placement", "Slot", "Row", "Column", "Flipped", "Amount"}
RULE_KEYS = {"startHour", "startMinute", "enemyRespawns", "respawnDelay"}
OPTIONAL_RULE_KEYS = {"inventory", "playerOutfit", "operatorOutfit", "eliteAI", "dayNightCycle", "weather"}

COMMON_WEATHER = (("dynamic", "Dynamic"), ("clear", "Clear"), ("overcast", "Overcast"))
TEMPERATE_WEATHER = (("fog", "Fog"), ("rain", "Rain"), ("thunderstorm", "Thunderstorm"))
MAP_WEATHER = {
    "chernarusplus": TEMPERATE_WEATHER,
    "enoch": TEMPERATE_WEATHER,
    "sakhal": (("snow", "Snow"), ("blizzard", "Blizzard")),
    "nasdara": (("high-wind", "High Wind"),),
}


def weather_options(map_id):
    return [{"id": code, "label": label} for code, label in COMMON_WEATHER + MAP_WEATHER[map_id]]


@cache
def inventory_catalog():
    if getattr(sys, "frozen", False):
        path = Path(sys._MEIPASS) / "predate-data/operation-catalog.json"
    else:
        path = Path(__file__).resolve().parents[1] / "launcher/resources/operation-catalog.json"
    base = json.loads(path.read_text(encoding="utf-8"))
    return {item["id"]: item for item in base + loadouts.modern_data()["Catalog"]}


def inventory(entries):
    if not isinstance(entries, list) or len(entries) > MAX_INVENTORY_ITEMS:
        raise ValueError(f"Starting inventory supports up to {MAX_INVENTORY_ITEMS} items")
    catalog = inventory_catalog() if entries else {}
    item_types, depths = [], []
    occupied_slots, occupied_cells = set(), set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict) or entry.keys() != ENTRY_KEYS:
            raise ValueError("Invalid starting inventory entry")
        item_class, parent = entry["ClassName"], entry["Parent"]
        if not isinstance(item_class, str) or item_class not in catalog:
            raise ValueError(f"Unknown starting inventory item: {item_class}")
        if type(parent) is not int or not -1 <= parent < index:
            raise ValueError(f"Invalid container for {item_class}")
        item = catalog[item_class]
        host = item_types[parent] if parent != -1 else None
        depth = depths[parent] + 1 if host else 1
        if depth > MAX_INVENTORY_DEPTH:
            raise ValueError(f"Too many levels of inventory nesting at {item_class}")

        placement, slot = entry["Placement"], entry["Slot"]
        row, col, flipped = entry["Row"], entry["Column"], entry["Flipped"]
        amount = entry["Amount"]
        if not isinstance(slot, str) or type(row) is not int or type(col) is not int or type(flipped) is not bool:
            raise ValueError(f"Invalid position for {item_class}")
        if type(amount) is not int or (
            amount != -1 and (
                item["capacity"] <= 0 or not (0 if item["kind"] == "magazine" else 1) <= amount <= item["capacity"]
            )
        ):
            raise ValueError(f"Invalid quantity for {item_class}")

        if placement == "cargo" and host and not slot:
            width, height = item["width"], item["height"]
            if flipped:
                width, height = height, width
            if not (0 <= row <= host["cargoHeight"] - height and 0 <= col <= host["cargoWidth"] - width):
                raise ValueError(f"{item_class} does not fit in {host['name']}")
            cells = {(parent, y, x) for y in range(row, row + height) for x in range(col, col + width)}
            if occupied_cells & cells:
                raise ValueError(f"Cargo overlaps in {host['name']}")
            occupied_cells.update(cells)
        elif (row, col, flipped) == (-1, -1, False):
            if placement == "hands" and not host and not slot:
                location = (parent, "hands")
            elif placement == "magazine" and host and not slot and any(
                item_class.lower() == magazine.lower() for magazine in host["magazines"]
            ):
                location = (parent, "magazine")
            elif placement == "attachment":
                allowed = {name.lower() for name in host["accepts"]} if host else PLAYER_SLOTS
                if slot.lower() not in allowed or slot.lower() not in {name.lower() for name in item["slots"]}:
                    raise ValueError(f"{item_class} is incompatible with {slot or 'that attachment slot'}")
                location = (parent, "attachment", slot.lower())
            else:
                raise ValueError(f"Invalid placement for {item_class}")
            if location in occupied_slots:
                raise ValueError(f"Two items occupy the same slot at {item_class}")
            occupied_slots.add(location)
        else:
            raise ValueError(f"Invalid cargo position for {item_class}")

        item_types.append(item)
        depths.append(depth)
    return entries


def config(selection, map_id=maps.DEFAULT_MAP):
    if not isinstance(selection, dict) or not RULE_KEYS <= selection.keys() <= RULE_KEYS | OPTIONAL_RULE_KEYS:
        raise ValueError("Invalid new operation settings")
    hour, minute = selection["startHour"], selection["startMinute"]
    if type(hour) is not int or type(minute) is not int or not (0 <= hour <= 23 and 0 <= minute <= 59):
        raise ValueError("Set a valid 24-hour start time")
    day_night_cycle = selection.get("dayNightCycle", True)
    if type(day_night_cycle) is not bool:
        raise ValueError("Set whether the day night cycle should run")
    respawns, delay = selection["enemyRespawns"], selection["respawnDelay"]
    if type(respawns) is not bool:
        raise ValueError("Set whether defeated enemies can redeploy")
    if type(delay) is not int or not 1 <= delay <= 3600:
        raise ValueError("Set a redeploy delay of 1–3600 seconds")
    rules = {
        "StartHour": hour,
        "StartMinute": minute,
        "FreezeTimeOfDay": not day_night_cycle,
        "EnableOperatorRespawns": respawns,
        "OperatorRespawnDelay": delay,
    }
    if "weather" in selection:
        weather = selection["weather"]
        if not isinstance(weather, str) or not any(option["id"] == weather for option in weather_options(map_id)):
            raise ValueError("Select weather available on the chosen map")
        rules["WeatherPreset"] = weather
    # Older launchers can send this field. Both values now use the same AI.
    if "eliteAI" in selection and type(selection["eliteAI"]) is not bool:
        raise ValueError("Invalid new operation settings")
    if "playerOutfit" in selection or "operatorOutfit" in selection:
        outfits = {outfit["Id"] for outfit in loadouts.definitions()["Outfits"]}
        for key, stored_name, allowed in (
            ("playerOutfit", "PlayerOutfit", outfits),
            ("operatorOutfit", "OperatorOutfit", outfits | {"varied"}),
        ):
            if key not in selection:
                continue
            outfit = selection[key]
            if not isinstance(outfit, str) or outfit not in allowed:
                raise ValueError("Select a valid clothing preset")
            rules[stored_name] = outfit
    if "inventory" in selection:
        rules["HasCustomInventory"] = True
        rules["PlayerInventory"] = inventory(selection["inventory"])
    return rules
