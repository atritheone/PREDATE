"""Identify optional mod content before opening an operation's saved world."""
from pathlib import Path
import hashlib

from app_runtime import ROOT
import release
import workshop_lasers


def optional_paths(values):
    if not isinstance(values, list) or any(not isinstance(value, str) for value in values):
        raise ValueError("Additional mods must be a list of folder paths")
    return [value for value in values if release.installed_realism_mod(value) is None]


def validate_paths(values):
    if not isinstance(values, list) or any(not isinstance(value, str) for value in values):
        raise ValueError("Additional mods must be a list of folder paths")
    seen = set()
    for value in values:
        path = Path(value)
        if ';' in value or not path.is_absolute() or not path.is_dir() or not list(path.glob('[Aa]ddons/*.pbo')):
            raise ValueError(f"Mod folder must contain addons/*.pbo and cannot contain a semicolon: {path}")
        resolved = str(path.resolve()).casefold()
        if resolved in seen:
            raise ValueError(f"Additional mod is selected more than once: {path}")
        seen.add(resolved)


def capture(values):
    validate_paths(values)
    mods = []
    for value in values:
        base = Path(value).resolve()
        files = {}
        for path in sorted(base.rglob('*')):
            if not path.is_file(): continue
            relative = path.relative_to(base)
            runtime = (relative.parts[0].casefold() in {'addons', 'keys'} and path.suffix.lower() in {'.pbo', '.bisign', '.bikey'})
            descriptor = len(relative.parts) == 1 and path.name.casefold() in {'mod.cpp', 'meta.cpp'}
            if not runtime and not descriptor: continue
            before = path.stat()
            with path.open('rb') as stream:
                digest = hashlib.file_digest(stream, 'sha256').hexdigest()
            after = path.stat()
            if (before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (after.st_size, after.st_mtime_ns, after.st_ctime_ns):
                raise RuntimeError(f"Mod changed while checking it. Let its update finish, then retry: {base.name}")
            files[relative.as_posix()] = digest
        mods.append({'path': str(base), 'files': files})
    return {'schema': 1, 'mods': mods}


def verify(values, expected):
    # Older hunts may have selected a Workshop copy of a now-required mod.
    # Verify its saved fingerprint against the staged copy used at launch.
    actual = capture([str(release.installed_realism_mod(value) or value) for value in values])
    if expected is None:
        # Legacy operations have no earlier content record to compare against.
        return actual
    if not isinstance(expected, dict) or expected.get('schema') != 1 or not isinstance(expected.get('mods'), list):
        raise ValueError('Unsupported operation mod record; the saved world was retained')
    if len(expected['mods']) != len(actual['mods']):
        raise ValueError('This operation\'s mod preset changed. Restore its recorded preset or start a new operation.')
    for previous, current in zip(expected['mods'], actual['mods']):
        if not isinstance(previous, dict) or not isinstance(previous.get('path'), str) or not isinstance(previous.get('files'), dict):
            raise ValueError('Invalid operation mod record; the saved world was retained')
        previous_path = release.installed_realism_mod(previous['path']) or Path(previous['path'])
        if previous_path.resolve() != Path(current['path']):
            raise ValueError('This operation\'s mod order or folders changed. Restore its recorded preset or start a new operation.')
        if previous['files'] != current['files']:
            if workshop_lasers.allows_companion_upgrade(ROOT, previous, current):
                continue
            names = sorted(name for name in set(previous['files']) | set(current['files']) if previous['files'].get(name) != current['files'].get(name))
            sample = ', '.join(names[:3])
            raise ValueError(f"Mod content changed for {Path(current['path']).name}: {sample}. Restore the recorded mod version or start a new operation; this saved world was not opened.")
    return actual


def install_keys(verified, destination):
    """Add verified keys by content, preserving existing keys with reused names."""
    destination = Path(destination)
    pending = {}
    for mod in verified['mods']:
        base = Path(mod['path'])
        for relative, expected in mod['files'].items():
            if not relative.lower().endswith('.bikey'): continue
            payload = (base/relative).read_bytes()
            if hashlib.sha256(payload).hexdigest() != expected:
                raise ValueError(f'Mod signing key changed after verification: {base.name}/{relative}')
            pending[expected] = payload
    if not pending: return
    destination.mkdir(parents=True, exist_ok=True)
    existing = set()
    for key in destination.glob('*.bikey'):
        with key.open('rb') as stream:
            existing.add(hashlib.file_digest(stream, 'sha256').hexdigest())
    for digest, payload in pending.items():
        if digest in existing: continue
        target = destination/f'PredateExtra-{digest}.bikey'
        if target.exists():
            raise ValueError(f'An existing mod key copy is damaged: {target.name}')
        # Exclusive creation cannot overwrite a standard or another mod's key.
        with target.open('xb') as stream:
            stream.write(payload)
