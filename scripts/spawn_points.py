"""Player insertion choices for the ChernarusPlus operation."""
import math
import random


# Specific presets are exact ground points. The native mission validates them.
PRESETS = {
    'original': ('PREDATE Starting Area', 4500, 6500),
    'cherno': ('Chernogorsk', 6063, 1932),
    'elektro': ('Elektrozavodsk', 8748, 2357),
    'kamyshovo': ('Kamyshovo', 11831, 3400),
    'berezino': ('Berezino', 12641, 9451),
    'svetlojarsk': ('Svetlojarsk', 14286, 13009),
    'southwest': ('Southwest • Zelenogorsk Outskirts', 2950, 4800),
    'west': ('West • Vybor Woods', 3200, 9250),
    'northwest': ('Northwest • Novaya Petrovka Outskirts', 3300, 12600),
    'central': ('Central • Kabanino Fields', 6050, 7650),
    'north': ('North • Severograd Outskirts', 7500, 12600),
    'east': ('East • Gorka Ridge', 10300, 9100),
    'northeast': ('Northeast • Turovo Outskirts', 13200, 13300),
}

# Select a region first so the density of coastal labels does not bias the draw.
REGIONS = (
    ('southwest',), ('west',), ('northwest',), ('central', 'original'), ('north',),
    ('east', 'berezino'), ('northeast', 'svetlojarsk'), ('cherno', 'elektro', 'kamyshovo'),
)


def options():
    return [{'id': key, 'name': label, 'x': x, 'z': z}
            for key, (label, x, z) in PRESETS.items()]


def validate(selection):
    if not isinstance(selection, dict):
        raise ValueError('Choose a spawn preset or enter Chernarus coordinates')
    if set(selection) == {'preset'}:
        key = selection['preset']
        if not isinstance(key, str) or (key != 'regional' and key not in PRESETS):
            raise ValueError('Unknown Chernarus spawn preset')
    elif set(selection) == {'x', 'z'}:
        x, z = selection['x'], selection['z']
        if any(type(value) not in (int, float) or not 10 < value < 15350 or not math.isfinite(value)
               for value in (x, z)):
            raise ValueError('Both Chernarus coordinates must be between 10 and 15350 metres')
    else:
        raise ValueError('Choose a spawn preset or enter Chernarus coordinates')


def config(selection):
    validate(selection)
    if 'preset' in selection:
        key = selection['preset']
        if key == 'original':
            return {}
        if key == 'regional':
            key = random.choice(random.choice(REGIONS))
        _, x, z = PRESETS[key]
    else:
        x, z = selection['x'], selection['z']
    return {'PlayerStart': [x, 0, z], 'ExplicitPlayerStart': True}
