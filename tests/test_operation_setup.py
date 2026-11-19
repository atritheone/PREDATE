import sys
"""New Operation validation covers the launcher-to-native configuration contract."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import backend_entry
import operation_setup
import session
import session_store as store


def entry(class_name, placement, *, parent=-1, slot="", row=-1, column=-1,
          flipped=False, amount=-1):
    return {"ClassName": class_name, "Placement": placement, "Parent": parent,
            "Slot": slot, "Row": row, "Column": column, "Flipped": flipped, "Amount": amount}


def settings(**overrides):
    return {"startHour": 20, "startMinute": 35, "enemyRespawns": False,
            "respawnDelay": 145, **overrides}


class OperationSetupTests(unittest.TestCase):
    def test_preferences_spawn_and_nested_inventory_are_written_together(self):
        items = [
            entry("Mich2001Helmet", "attachment", slot="Headgear"),
            entry("NVGoggles", "attachment", parent=0, slot="NVG"),
            entry("Battery9V", "attachment", parent=1, slot="BatteryD", amount=45),
            entry("M4A1", "hands"),
            entry("Mag_STANAG_30Rnd", "magazine", parent=3, amount=12),
            entry("AssaultBag_Green", "attachment", slot="Back"),
            entry("BandageDressing", "cargo", parent=5, row=1, column=2, flipped=True, amount=3),
        ]
        with tempfile.TemporaryDirectory() as directory, patch.object(store, "ROOT", Path(directory)), \
             patch("world.select_profile"), patch("world.setup"):
            store.create_operation("Night operation", profile="op-night", extra_mods=[],
                                   spawn={"x": 4500.25, "z": 6500.75}, opponent_count=15,
                                   footprints=True, setup=settings(inventory=items))
            config = store.read_json(store.world_path("op-night") / "profile/PredateConfig.json")
        self.assertEqual(config["PlayerStart"], [4500.25, 0, 6500.75])
        self.assertEqual(config["OpponentCount"], 15)
        self.assertTrue(config["EnableFootprints"])
        self.assertEqual((config["StartHour"], config["StartMinute"]), (20, 35))
        self.assertFalse(config["EnableOperatorRespawns"])
        self.assertEqual(config["OperatorRespawnDelay"], 145)
        self.assertTrue(config["HasCustomInventory"])
        self.assertEqual(config["PlayerInventory"], items)

    def test_empty_custom_inventory_is_distinct_from_using_a_preset(self):
        self.assertNotIn("PlayerInventory", operation_setup.config(settings()))
        empty = operation_setup.config(settings(inventory=[]))
        self.assertTrue(empty["HasCustomInventory"])
        self.assertEqual(empty["PlayerInventory"], [])

    def test_invalid_inventory_never_creates_a_world(self):
        pack = entry("AssaultBag_Green", "attachment", slot="Back")
        bandage = entry("BandageDressing", "cargo", parent=0, row=0, column=0)
        invalid = [
            ([pack, bandage, bandage], "Cargo overlaps"),
            ([pack, entry("BandageDressing", "cargo", parent=0, row=6, column=0)], "does not fit"),
            ([entry("NVGoggles", "attachment", slot="Eyewear")], "incompatible"),
            ([entry("NVGoggles", "attachment", parent=0, slot="NVG")], "Invalid container"),
            ([entry("UnknownItem", "hands")], "Unknown starting inventory item"),
            ([entry("Mag_STANAG_30Rnd", "hands", amount=31)], "Invalid quantity"),
        ]
        with tempfile.TemporaryDirectory() as directory, patch.object(store, "ROOT", Path(directory)), \
             patch("world.setup") as setup:
            for entries, message in invalid:
                with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                    store.create_operation("Invalid", profile="op-invalid", extra_mods=[],
                                           setup=settings(inventory=entries))
            setup.assert_not_called()
            self.assertFalse(store.world_path("op-invalid").exists())

    def test_bridge_validates_new_game_rules_before_queueing_the_request(self):
        with patch("session.request", return_value="id") as request, patch("spawn_points.random.choice") as draw:
            with self.assertRaisesRegex(ValueError, "only to a new operation"):
                backend_entry.dispatch("action", {"action": "continue", "setup": settings()})
            for rules in (settings(startHour=24), settings(respawnDelay=0), settings(enemyRespawns=1)):
                with self.assertRaises(ValueError):
                    backend_entry.dispatch("action", {"action": "new", "setup": rules})
            request.assert_not_called()
            values = {"name": "Valid", "setup": settings(), "spawn": {"preset": "regional"}}
            self.assertEqual(backend_entry.dispatch("action", {"action": "new", **values}), {"requestId": "id"})
            request.assert_called_once_with("new", **values)
            draw.assert_not_called()

    def test_session_does_not_lose_settings_to_an_older_supervisor(self):
        with patch("session.ensure_manager"), patch("session.state", return_value={}), \
             patch("session.time.monotonic", side_effect=[1, 4]), patch("session.store.atomic_json") as write:
            with self.assertRaisesRegex(RuntimeError, "older PREDATE session"):
                session.request("new", name="Night", setup=settings())
            write.assert_not_called()


if __name__ == "__main__":
    unittest.main()
