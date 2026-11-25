"""Client presentation preferences, separate from saved operation rules."""
from pathlib import Path


def validate(preferences):
    scale = preferences.get('receiver_scale', 100)
    contrast = preferences.get('receiver_high_contrast', False)
    if type(scale) is not int or not 80 <= scale <= 150:
        raise ValueError('Compass size must be between 80 and 150 percent')
    if not isinstance(contrast, bool):
        raise ValueError('Compass high contrast must be true or false')
    return {'Schema':1, 'CompassPercent':scale, 'HighContrast':contrast}


def write_client_options(directory, preferences, save_name=""):
    import session_store as store
    options = validate(preferences)
    if save_name:
        options['SaveName'] = save_name
    store.atomic_json(Path(directory)/'PredatePresentation.json', options)
