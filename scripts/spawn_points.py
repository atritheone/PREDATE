"""Player insertion choices for a hunt's selected terrain."""
import math
import random
import maps


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


LIVONIA_PRESETS = {
    "original": ("PREDATE Starting Area", 5400, 6350),
    "topolin": ("Topolin Outskirts", 2189, 7677),
    "radacz": ("Radacz Outskirts", 4034, 8266),
    "gliniska": ("Gliniska Outskirts", 5211, 10043),
    "brena": ("Brena Outskirts", 6648, 11373),
    "kolembrody": ("Kolembrody Outskirts", 8353, 11714),
    "east": ("East • Sitnik Fields", 11600, 9700),
    "southwest": ("Southwest • Forest Edge", 2550, 3750),
    "south": ("South • Nadbór Woods", 6600, 4200),
    "southeast": ("Southeast • Hills", 10100, 3500),
}

LIVONIA_REGIONS = (
    ("topolin",), ("radacz", "original"), ("gliniska", "brena"), ("kolembrody", "east"),
    ("southwest",), ("south",), ("southeast",),
)

SAKHAL_PRESETS = {
    "original": ("PREDATE Starting Area", 9105, 9735),
    "petropavlovsk": ("Petropavlovsk-Sakhalskiy Outskirts", 5055, 10215),
    "bolotnoe": ("Bolotnoe Ridge", 5625, 8955),
    "dudino": ("Dudino Outskirts", 6495, 7500),
    "yuzhnoe": ("Yuzhnoe Outskirts", 10875, 6465),
    "sakhalag": ("Sakhalag Hills", 11721, 9869),
    "rudnogorsk": ("Rudnogorsk Outskirts", 13995, 12120),
    "tumanovo": ("Tumanovo Foothills", 8805, 13005),
    "aniva": ("Aniva Outskirts", 12705, 7410),
}

NASDARA_PRESETS = {
    "original": ("PREDATE Starting Area", 7536, 9008),
    "dashtabad": ("Dashtabad Outskirts", 10395, 10747),
    "pesh-kuh": ("Pesh Kuh Foothills", 3632, 11056),
    "bagh-e-mir": ("Bagh-e Mir Outskirts", 4816, 5744),
    "no-abad": ("No Abad Fields", 6992, 5392),
    "gard-abad": ("Gard Abad Hills", 10576, 7440),
    "dara-e-nur": ("Dara-e Nur Foothills", 10800, 13776),
    "marz-e-lut": ("Marz-e Lut Outskirts", 13744, 12703),
    "dahaneh-e-kavir": ("Dahaneh-e Kavir Outskirts", 11376, 2992),
    "chashme-sabz": ("Chashme Sabz Outskirts", 6960, 12592),
}


def presets_for(map_id):
    selected = maps.terrain(map_id)
    if selected.id == "enoch":
        return LIVONIA_PRESETS, LIVONIA_REGIONS
    if selected.id == "sakhal":
        return SAKHAL_PRESETS, (("petropavlovsk", "bolotnoe"), ("original", "tumanovo"),
                               ("yuzhnoe", "dudino"), ("aniva", "sakhalag"), ("rudnogorsk",))
    if selected.id == "nasdara":
        return NASDARA_PRESETS, (("original", "gard-abad"), ("pesh-kuh", "chashme-sabz"),
                                ("bagh-e-mir", "no-abad"), ("dashtabad", "marz-e-lut"),
                                ("dara-e-nur",), ("dahaneh-e-kavir",))
    return PRESETS, REGIONS


def options(map_id=maps.DEFAULT_MAP):
    presets, _ = presets_for(map_id)
    return [{'id': key, 'name': label, 'x': x, 'z': z}
            for key, (label, x, z) in presets.items()]


def validate(selection, map_id=maps.DEFAULT_MAP):
    selected = maps.terrain(map_id)
    presets, _ = presets_for(map_id)
    if not isinstance(selection, dict):
        raise ValueError(f'Choose a spawn preset or enter {selected.name} coordinates')
    if set(selection) == {'preset'}:
        key = selection['preset']
        if not isinstance(key, str) or (key != 'regional' and key not in presets):
            raise ValueError(f'Unknown {selected.name} spawn preset')
    elif set(selection) == {'x', 'z'}:
        x, z = selection['x'], selection['z']
        if any(type(value) not in (int, float) or not 10 < value < selected.size - 10 or not math.isfinite(value)
               for value in (x, z)):
            raise ValueError(f'Both {selected.name} coordinates must be between 10 and {selected.size - 10} metres')
    else:
        raise ValueError(f'Choose a spawn preset or enter {selected.name} coordinates')


def config(selection, map_id=maps.DEFAULT_MAP):
    validate(selection, map_id)
    presets, regions = presets_for(map_id)
    if 'preset' in selection:
        key = selection['preset']
        if key == 'original' and map_id == maps.DEFAULT_MAP:
            return {}
        if key == 'regional':
            key = random.choice(random.choice(regions))
        _, x, z = presets[key]
    else:
        x, z = selection['x'], selection['z']
    return {'PlayerStart': [x, 0, z], 'ExplicitPlayerStart': True}
