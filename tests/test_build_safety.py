import sys
"""An imported build helper must not overwrite content loaded by a game."""
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import build


class BuildSafetyTests(unittest.TestCase):
    def test_loaded_runtimes_block_before_writing(self):
        for executable in ('DayZ_x64.exe', 'DayZServer_x64.exe', 'DayZDiag_x64.exe'):
            for helper in (build.pack, build.mission, build.runtime):
                with self.subTest(executable=executable, helper=helper.__name__):
                    with patch('platform_win.find_processes', side_effect=lambda name: [123] if name == executable else []), patch('build.layouts') as layouts:
                        with self.assertRaisesRegex(RuntimeError, executable.replace('.', r'\.')):
                            helper()
                        layouts.assert_not_called()

    def test_filebank_never_receives_an_existing_output(self):
        with TemporaryDirectory() as folder:
            root = Path(folder)
            addons = root / 'mods/@PredateCandidate/addons'
            addons.mkdir(parents=True)
            (root / 'runtime').mkdir()
            pbo = addons / 'Predate.pbo'
            pbo.write_bytes(b'previous build')
            signing = root / 'private-signing'
            signing.mkdir()
            for suffix in ('bikey', 'biprivatekey'):
                (signing / f'PredateLocal.{suffix}').touch()
            (root / 'config').mkdir()
            (root / 'scripts').mkdir()
            for name in ('scripts/build.py', 'scripts/release.py', 'scripts/world.py', 'config/release-lock.json'):
                (root / name).touch()

            def tool(args, cwd=None):
                if args[0].name == 'FileBank.exe':
                    output = args[args.index('-dst') + 1] / pbo.name
                    self.assertFalse(output.exists(), 'FileBank can silently leave existing output intact')
                    output.write_bytes(b'fresh build')
                else:
                    args[-1].with_name(pbo.name + '.PredateLocal.bisign').write_bytes(b'signature')

            with patch.object(build, 'ROOT', root), patch.object(build.release, 'LOCK', root / 'config/release-lock.json'), patch.object(build.release, 'source_fingerprint', return_value={}), patch.object(build, 'layouts'), patch.object(build, 'require_closed_runtime'), patch.object(build, 'run', side_effect=tool):
                build.pack(candidate=True)
            self.assertEqual(pbo.read_bytes(), b'fresh build')


if __name__ == '__main__':
    unittest.main()
