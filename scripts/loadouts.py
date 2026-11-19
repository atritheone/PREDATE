"""Starting-kit catalog shared by the launcher and the native addon."""
from pathlib import Path
import json
import sys

from app_runtime import ROOT


def catalog():
    path = Path(sys._MEIPASS)/'predate-data/loadouts.json' if getattr(sys, 'frozen', False) else ROOT/'source/Predate/data/loadouts.json'
    data = json.loads(path.read_text(encoding='utf-8'))
    if data.get('Schema') != 1 or not isinstance(data.get('Kits'), list) or not 1 <= len(data['Kits']) <= 16:
        raise ValueError('Unsupported starting-kit catalog')
    operator_kits = data.get('OperatorKits', [])
    if not isinstance(operator_kits, list) or len(operator_kits) > 16:
        raise ValueError('Unsupported operator kit catalog')
    kits = data['Kits'] + operator_kits
    ids = set()
    for kit in kits:
        if not isinstance(kit, dict) or any(not isinstance(kit.get(field), str) or not kit[field] for field in ('Id','Name','Description','FirearmClass','MagazineClass')):
            raise ValueError('Invalid starting-kit definition')
        if kit['Id'] in ids or not isinstance(kit.get('SpareMagazines'), int) or not 0 <= kit['SpareMagazines'] <= 12:
            raise ValueError('Invalid or duplicate starting-kit definition')
        if not isinstance(kit.get('Attachments'), list) or any(not isinstance(item, str) or not item for item in kit['Attachments']):
            raise ValueError('Invalid starting-kit attachments')
        ids.add(kit['Id'])
    return kits


def get(identifier):
    if not isinstance(identifier, str): raise ValueError('Select a starting kit')
    for kit in catalog():
        if kit['Id'] == identifier: return kit
    raise ValueError('Unknown starting kit: ' + identifier)


def options():
    return [{'id':kit['Id'], 'name':kit['Name'], 'description':kit['Description']} for kit in catalog()]
