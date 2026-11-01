import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from tools.release.preview_package import package_tree, verify_release


class PreviewPackageTest(unittest.TestCase):
    def test_manifest_and_archive_preserve_exact_assets(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = root / 'stage'
            (stage / 'sdk').mkdir(parents=True)
            (stage / 'sdk' / 'runtime.lib').write_bytes(b'library')
            (stage / 'README.md').write_text('consumer notes\n', encoding='utf-8')
            archive = root / 'asset.zip'
            package_tree(stage, archive, ['README.md', 'sdk/runtime.lib'], 'a' * 40, 'windows-sdk')
            with zipfile.ZipFile(archive) as z:
                self.assertEqual(z.read('sdk/runtime.lib'), b'library')
                manifest = json.loads(z.read('manifest.json'))
                self.assertEqual(manifest['source_revision'], 'a' * 40)
                self.assertEqual(manifest['files']['sdk/runtime.lib'], hashlib.sha256(b'library').hexdigest())

    def test_missing_required_file_does_not_create_archive(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = root / 'stage'
            stage.mkdir()
            with self.assertRaises(ValueError):
                package_tree(stage, root / 'bad.zip', ['required.exe'], 'b' * 40, 'windows-demo')
            self.assertFalse((root / 'bad.zip').exists())

    def test_rejects_symlink_and_invalid_revision(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = root / 'stage'
            stage.mkdir()
            (stage / 'ok').write_bytes(b'ok')
            with self.assertRaises(ValueError):
                package_tree(stage, root / 'bad.zip', ['ok'], 'not-a-revision', 'web-demo')
            (stage / 'link').write_bytes(b'link')
            with patch.object(Path, 'is_symlink', lambda path: path.name == 'link'):
                with self.assertRaises(ValueError):
                    package_tree(stage, root / 'bad.zip', ['ok'], 'c' * 40, 'web-demo')

    def test_release_requires_four_distinct_assets_for_exact_revision(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = root / 'stage'
            stage.mkdir()
            (stage / 'payload').write_bytes(b'payload')
            release = root / 'release'
            release.mkdir()
            for platform in ('windows', 'web'):
                for kind in ('sdk', 'demo'):
                    package_tree(stage, release / f'axiom-{kind}-{platform}-preview.zip',
                                 ['payload'], 'd' * 40, f'{platform}-{kind}')
            verify_release(release, 'd' * 40)
            self.assertEqual(len((release / 'SHA256SUMS').read_text().splitlines()), 4)
            with self.assertRaises(ValueError):
                verify_release(release, 'd' * 40, require_js=True)
            package_tree(stage, release / 'axiom-sdk-js-preview.zip', ['payload'], 'd' * 40, 'js-sdk')
            verify_release(release, 'd' * 40, require_js=True)
            self.assertEqual(len((release / 'SHA256SUMS').read_text().splitlines()), 5)
            (release / 'axiom-sdk-js-preview.zip').unlink()
            with self.assertRaises(ValueError):
                verify_release(release, 'e' * 40)
            (release / 'axiom-demo-web-preview.zip').unlink()
            with self.assertRaises(ValueError):
                verify_release(release, 'd' * 40)


if __name__ == '__main__':
    unittest.main()
