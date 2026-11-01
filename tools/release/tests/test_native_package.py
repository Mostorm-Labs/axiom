import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[3]


class NativePackageTest(unittest.TestCase):
    def setUp(self):
        self.source = patch('tools.release.native_package.source_identity', return_value=('a'*40, False))
        self.source.start()
        self.addCleanup(self.source.stop)

    def module(self):
        self.assertIsNotNone(importlib.util.find_spec('tools.release.native_package'),
                             'missing isolated RN/native consumer packager')
        from tools.release import native_package
        return native_package

    def fixture(self, root):
        stage = root / 'native'
        for name in ['lib/libaxiom_native.so', 'include/axiom/native.h',
                     'assets/axiom-font.otf', 'assets/fonts/catalog.json',
                     'licenses/skia/Skia.txt', 'licenses/semantic/Protobuf.txt']:
            path = stage / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture')
        path = stage / 'share/axiom-native/version.json'
        path.parent.mkdir(parents=True)
        path.write_text((REPO / 'sdk/version.json').read_text())
        self.provenance(stage)
        return stage

    def provenance(self, stage, target='android-arm64-v8a'):
        files={p.relative_to(stage).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
               for p in stage.rglob('*') if p.is_file() and p.name!='native-manifest.json'}
        (stage/'native-manifest.json').write_text(json.dumps({'source_revision':'a'*40,
          'sdk_version':json.loads((REPO/'sdk/version.json').read_text())['version'],
          'target':target,'dirty':False,'files':files}))

    def test_stage_has_self_contained_shared_api_and_native_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = self.fixture(root)
            result = self.module().stage_react_native(REPO, {'android-arm64-v8a': stage},
                                                      root / 'package', 'a' * 40)
            package = root / 'package'
            self.assertEqual(result['source_revision'], 'a' * 40)
            self.assertTrue((package / 'native/android/lib/arm64-v8a/libaxiom_native.so').is_file())
            self.assertTrue((package / 'api/internal/transport.mjs').is_file())
            source = (package / 'src/controller.ts').read_text()
            self.assertIn("from '../api/axiom.mjs'", source)
            self.assertNotIn('../../api/', source)
            metadata = json.loads((package / 'package.json').read_text())
            self.assertEqual(metadata['version'], result['sdk_version'])
            self.assertEqual(set(metadata['exports']), {'.'})
            self.assertFalse(list(package.rglob('*.a')))
            self.assertFalse((package / 'native/android/include/session.hpp').exists())

    def test_internal_archives_or_foreign_version_reject_before_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = self.fixture(root)
            internal = stage / 'lib/libcanvas_runtime.a'
            internal.write_bytes(b'internal')
            with self.assertRaises(ValueError):
                self.module().stage_react_native(REPO, {'android-arm64-v8a': stage}, root / 'one', 'a' * 40)
            self.assertFalse((root / 'one').exists())
            internal.unlink()
            (stage / 'share/axiom-native/version.json').write_text('{"version":"9.0.0"}')
            with self.assertRaises(ValueError):
                self.module().stage_react_native(REPO, {'android-arm64-v8a': stage}, root / 'two', 'a' * 40)
            self.assertFalse((root / 'two').exists())

    def test_no_platforms_and_overwrite_reject(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaises(ValueError):
                self.module().stage_react_native(REPO, {}, root / 'output', 'a' * 40)
            stage = self.fixture(root)
            existing = root / 'output'
            existing.mkdir()
            (existing / 'keep').write_text('user')
            with self.assertRaises(ValueError):
                self.module().stage_react_native(REPO, {'android-arm64-v8a': stage}, existing, 'a' * 40)
            self.assertEqual((existing / 'keep').read_text(), 'user')

    def test_rejects_mixed_resource_sets_before_creating_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            first = self.fixture(root / 'one')
            second = self.fixture(root / 'two')
            (second / 'assets/axiom-font.otf').write_bytes(b'foreign')
            self.provenance(second,'android-x86_64')
            with self.assertRaises(ValueError):
                self.module().stage_react_native(REPO, {'android-arm64-v8a': first, 'android-x86_64': second}, root / 'output', 'a' * 40)
            self.assertFalse((root / 'output').exists())

    def test_native_source_revision_cannot_be_relabelled_by_staging(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = self.fixture(root)
            (stage / 'native-manifest.json').write_text(json.dumps({
                'source_revision':'b'*40, 'sdk_version':json.loads((REPO/'sdk/version.json').read_text())['version'],
                'target':'android-arm64-v8a', 'dirty':False, 'files':{}}))
            with self.assertRaises(ValueError):
                self.module().stage_react_native(REPO, {'android-arm64-v8a': stage}, root / 'output', 'a'*40)
            self.assertFalse((root/'output').exists())

    def apple_fixture(self, root, target):
        stage = self.fixture(root)
        (stage / 'lib/libaxiom_native.so').unlink()
        framework = stage / 'lib/AxiomNative.framework'
        framework.mkdir()
        (framework / 'AxiomNative').write_bytes(target.encode())
        self.provenance(stage, target)
        return stage

    def test_ios_device_and_simulator_are_assembled_for_podspec(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            installs = {t: self.apple_fixture(root / t, t)
                        for t in ['ios-arm64', 'ios-simulator-arm64']}
            def assemble(command, **kwargs):
                self.assertEqual(command[:2], ['xcodebuild', '-create-xcframework'])
                self.assertEqual(command.count('-framework'), 2)
                output = Path(command[command.index('-output') + 1])
                output.mkdir(parents=True)
                (output / 'Info.plist').write_bytes(b'assembled')
            with patch('tools.release.native_package.subprocess.run', side_effect=assemble):
                result = self.module().stage_react_native(REPO, installs, root / 'output', 'a'*40)
            name = 'native/apple/AxiomNative.xcframework/Info.plist'
            self.assertIn(name, result['files'])
            self.assertFalse((root / 'output/native/ios').exists())

    def test_apple_assets_keep_directory_identity_for_cocoapods(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = self.apple_fixture(root, 'macos-arm64')
            for family in ['one', 'two']:
                path = stage / 'assets/fonts' / family / 'font.otf'
                path.parent.mkdir()
                path.write_bytes(family.encode())
            self.provenance(stage, 'macos-arm64')
            result = self.module().stage_react_native(REPO, {'macos-arm64': stage}, root / 'output', 'a'*40)
            for family in ['one', 'two']:
                name = 'native/AxiomAssets.bundle/fonts/' + family + '/font.otf'
                self.assertIn(name, result['files'])
                self.assertEqual((root / 'output' / name).read_bytes(), family.encode())
            self.assertIn("s.resources = 'native/AxiomAssets.bundle'",
                          (root / 'output/AxiomReactNative.podspec').read_text())

    def test_incomplete_ios_pair_and_failed_assembly_leave_no_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            installs = {'ios-arm64': self.apple_fixture(root / 'device', 'ios-arm64')}
            with self.assertRaises(ValueError):
                self.module().stage_react_native(REPO, installs, root / 'output', 'a'*40)
            self.assertFalse((root / 'output').exists())

            installs['ios-simulator-arm64'] = self.apple_fixture(root / 'sim', 'ios-simulator-arm64')
            with patch('tools.release.native_package.subprocess.run', side_effect=RuntimeError('xcodebuild')):
                with self.assertRaises(RuntimeError):
                    self.module().stage_react_native(REPO, installs, root / 'output', 'a'*40)
            self.assertFalse((root / 'output').exists())

    def test_dirty_or_foreign_source_is_not_relabelled(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stage = self.fixture(root)
            for identity in [('b'*40, False), ('a'*40, True)]:
                with patch('tools.release.native_package.source_identity', return_value=identity):
                    with self.assertRaises(ValueError):
                        self.module().stage_react_native(REPO, {'android-arm64-v8a': stage}, root / 'output', 'a'*40)
                self.assertFalse((root / 'output').exists())
            provenance = json.loads((stage / 'native-manifest.json').read_text())
            provenance['dirty'] = True
            (stage / 'native-manifest.json').write_text(json.dumps(provenance))
            with self.assertRaises(ValueError):
                self.module().stage_react_native(REPO, {'android-arm64-v8a': stage}, root / 'output', 'a'*40)
            self.assertFalse((root / 'output').exists())
