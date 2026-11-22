"""Starting kits from the native catalog and the launcher Workshop presets."""
from functools import cache
from pathlib import Path
import json
import sys

from app_runtime import ROOT
from build import WORKSHOP


@cache
def modern_data():
    resources = (Path(sys._MEIPASS) / "predate-data" if getattr(sys, "frozen", False)
                 else Path(__file__).resolve().parents[1] / "launcher/resources")
    return json.loads((resources / "modern-loadouts.json").read_text(encoding="utf-8"))


def definitions():
    path = Path(sys._MEIPASS)/'predate-data/loadouts.json' if getattr(sys, 'frozen', False) else ROOT/'source/Predate/data/loadouts.json'
    return json.loads(path.read_text(encoding='utf-8'))


def catalog():
    data = definitions()
    if data.get('Schema') != 1 or not isinstance(data.get('Kits'), list) or not 1 <= len(data['Kits']) <= 16:
        raise ValueError('Unsupported starting-kit catalog')
    operator_kits = data.get('OperatorKits', [])
    if not isinstance(operator_kits, list) or len(operator_kits) > 16:
        raise ValueError('Unsupported operator kit catalog')
    modern = modern_data()
    presets = [{**modern["Equipment"], **kit, "PlayerPreset": True} for kit in modern["Kits"]]
    kits = presets + data['Kits'] + operator_kits
    ids = set()
    for kit in kits:
        if not isinstance(kit, dict) or any(not isinstance(kit.get(field), str) or not kit[field] for field in ('Id','Name','Description','FirearmClass','MagazineClass')):
            raise ValueError('Invalid starting-kit definition')
        if kit['Id'] in ids or not isinstance(kit.get('SpareMagazines'), int) or not 0 <= kit['SpareMagazines'] <= 12:
            raise ValueError('Invalid or duplicate starting-kit definition')
        if not isinstance(kit.get('Attachments'), list) or any(not isinstance(item, str) or not item for item in kit['Attachments']):
            raise ValueError('Invalid starting-kit attachments')
        items = kit.get('WorkshopItems', [])
        if not isinstance(items, list) or any(not isinstance(item, str) or not item.isascii() or not item.isdecimal() for item in items):
            raise ValueError('Invalid starting-kit Workshop items')
        ids.add(kit['Id'])
    return kits


def get(identifier):
    if not isinstance(identifier, str): raise ValueError('Select a starting kit')
    for kit in catalog():
        if kit['Id'] == identifier: return kit
    raise ValueError('Unknown starting kit: ' + identifier)


def workshop_paths(kit, selected=()):
    folders = {Path(value).name: Path(value) for value in selected}
    paths = []
    for item in kit.get('WorkshopItems', []):
        path = folders.get(item, WORKSHOP / item)
        if not list(path.glob('[Aa]ddons/*.pbo')) or not list(path.glob('[Kk]eys/*.bikey')):
            raise ValueError(f'{kit["Name"]} needs installed Steam Workshop item {item}')
        paths.append(path)
    return paths


def options(selected=()):
    result = []
    modern = []
    for kit in catalog():
        try:
            packs = workshop_paths(kit, selected)
        except ValueError:
            continue
        option = {'id': kit['Id'], 'name': kit['Name'], 'description': kit['Description']}
        if packs:
            option['workshop'] = True
        (modern if kit.get('PlayerPreset') else result).append(option)
    return modern or result
