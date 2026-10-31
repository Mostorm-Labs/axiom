import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.release.consumer_package import JS_REQUIRED

REPO = Path(__file__).resolve().parents[3]
REVISION = 'd' * 40
REGISTRY = 'http://npm.nearhub.cc/'


class NpmPublicationTest(unittest.TestCase):
    def test_package_is_publishable_only_to_the_requested_scope_and_registry(self):
        package = json.loads((REPO / 'sdk/web/package.json').read_text())
        self.assertEqual(package['name'], '@axiom/axiom')
        self.assertIs(package.get('private'), False)
        self.assertEqual(package['publishConfig'], {'registry': REGISTRY, 'tag': 'preview'})
        self.assertIn('manifest.json', package['files'])
        self.assertEqual(package['version'], json.loads((REPO / 'sdk/version.json').read_text())['version'])

    def prepare(self, archive, output, revision=REVISION):
        spec = importlib.util.find_spec('tools.release.npm_publish')
        self.assertIsNotNone(spec, 'missing verified Release-to-npm preparation entry')
        from tools.release.npm_publish import prepare_package
        return prepare_package(archive, output, revision)

    def archive(self, root, *, metadata=None, revision=REVISION, kind='js-sdk',
                extra=None, corrupt=None, version='1.0.0-preview.1'):
        package = json.loads((REPO / 'sdk/web/package.json').read_text())
        package.update(metadata or {})
        payload = {name: b'verification fixture, not product evidence' for name in JS_REQUIRED}
        payload['licenses/semantic/Protobuf.txt'] = b'license fixture'
        payload['package.json'] = json.dumps(package).encode()
        payload.update(extra or {})
        manifest = {'format': 'axiom-sdk-preview-v1', 'kind': kind,
                    'source_revision': revision, 'sdk_version': version,
                    'files': {k: hashlib.sha256(v).hexdigest() for k, v in payload.items()}}
        if corrupt:
            payload[corrupt] += b'tampered'
        archive = root / 'sdk.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            for name, data in payload.items():
                z.writestr(name, data)
            z.writestr('manifest.json', json.dumps(manifest))
        return archive

    def test_verified_release_bytes_are_extracted_without_rewriting(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); archive = self.archive(root); output = root / 'npm'
            result = self.prepare(archive, output)
            self.assertEqual(result['name'], '@axiom/axiom')
            with zipfile.ZipFile(archive) as z:
                for name in z.namelist():
                    self.assertEqual((output / name).read_bytes(), z.read(name))

    def test_rejects_corrupt_bytes_before_creating_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); archive = self.archive(root, corrupt='axiom.wasm')
            with self.assertRaisesRegex(ValueError, 'hash'):
                self.prepare(archive, root / 'npm')
            self.assertFalse((root / 'npm').exists())

    def test_rejects_wrong_source_kind_or_version(self):
        for options in ({'revision': 'e'*40}, {'kind': 'web-demo'}, {'version': '1.0.0-preview.2'}):
            with self.subTest(options=options), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); archive = self.archive(root, **options)
                with self.assertRaisesRegex(ValueError, 'identity|version'):
                    self.prepare(archive, root / 'npm')

    def test_rejects_foreign_registry_scope_private_and_lifecycle_scripts(self):
        for metadata in ({'name': '@other/axiom'}, {'private': True},
                         {'publishConfig': {'registry': 'https://registry.npmjs.org/', 'tag': 'preview'}},
                         {'scripts': {'prepublishOnly': 'untrusted'}}):
            with self.subTest(metadata=metadata), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); archive = self.archive(root, metadata=metadata)
                with self.assertRaises(ValueError):
                    self.prepare(archive, root / 'npm')

    def test_rejects_traversal_duplicates_symlinks_and_credentials(self):
        for name in ('../escape', '/escape', 'x\\escape', '.npmrc', 'nested/.npmrc', '.env', '.env.local'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); archive = self.archive(root, extra={name: b'forbidden'})
                with self.assertRaises(ValueError):
                    self.prepare(archive, root / 'npm')
                self.assertFalse((root / 'npm').exists())
        for duplicate in (False, True):
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); archive = self.archive(root)
                with zipfile.ZipFile(archive, 'a') as z:
                    info = zipfile.ZipInfo('link' if not duplicate else 'axiom.mjs')
                    info.create_system = 3; info.external_attr = 0o120777 << 16
                    z.writestr(info, b'axiom.mjs')
                with self.assertRaises(ValueError):
                    self.prepare(archive, root / 'npm')

    def test_never_overwrites_existing_work(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); archive = self.archive(root); output = root / 'npm'
            output.mkdir(); (output / 'keep').write_text('user work')
            with self.assertRaises(ValueError):
                self.prepare(archive, output)
            self.assertEqual((output / 'keep').read_text(), 'user work')

    def test_rejects_stable_version_and_missing_npm_provenance(self):
        for metadata in ({'version': '1.0.0'}, {'files': ['*.mjs']}):
            with self.subTest(metadata=metadata), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); archive = self.archive(root, metadata=metadata)
                with self.assertRaises(ValueError):
                    self.prepare(archive, root / 'npm')

    def test_rejects_unmanifested_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); archive = self.archive(root)
            with zipfile.ZipFile(archive, 'a') as z:
                z.writestr('unexpected.txt', b'not bound to the release')
            with self.assertRaisesRegex(ValueError, 'file set'):
                self.prepare(archive, root / 'npm')

    def test_rejects_missing_required_assets_even_with_consistent_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); archive = self.archive(root)
            with zipfile.ZipFile(archive) as z:
                files = {name: z.read(name) for name in z.namelist() if name != 'axiom.wasm'}
            manifest = json.loads(files['manifest.json'])
            del manifest['files']['axiom.wasm']
            files['manifest.json'] = json.dumps(manifest).encode()
            with zipfile.ZipFile(archive, 'w') as z:
                for name, content in files.items():
                    z.writestr(name, content)
            with self.assertRaisesRegex(ValueError, 'missing required'):
                self.prepare(archive, root / 'npm')


if __name__ == '__main__':
    unittest.main()
