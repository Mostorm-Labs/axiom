"""Version identity must survive SDK generation, packaging and CI retries."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.release import preview_package

REPO = Path(__file__).resolve().parents[3]


class VersionIdentityTest(unittest.TestCase):
    def test_product_version_and_npm_version_are_separate(self):
        from tools.release.version_identity import identity
        value = identity('1.3.4', 1234)
        self.assertEqual(value, {'version': '1.3.4', 'buildNumber': 1234,
                                 'displayVersion': '1.3.4.1234'})

    def test_rejects_untransportable_numbers_and_non_semver(self):
        from tools.release.version_identity import identity
        for build in (-1, True, 1.5, '1', 9007199254740992):
            with self.subTest(build=build), self.assertRaises(ValueError):
                identity('1.3.4', build)
        for version in ('1.3.4.5', '01.3.4', '1.3.4-preview.1', '1.3.4+5'):
            with self.subTest(version=version), self.assertRaises(ValueError):
                identity(version, 1)

    def test_generated_api_and_packages_match_a_build_without_source_mutation(self):
        from tools.release.version_identity import render_sdk_files
        inputs = ['sdk/api/axiom.mjs', 'sdk/api/axiom.d.mts',
                  'sdk/api/internal/transport.d.mts', 'sdk/web/package.json']
        before = {p: (REPO / p).read_bytes() for p in inputs}
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            render_sdk_files(REPO, output, 1234)
            package = json.loads((output / 'package.json').read_text())
            version = json.loads((REPO / 'sdk/version.json').read_text())['version']
            self.assertEqual(package['version'], version)
            self.assertEqual(package['buildNumber'], 1234)
            module = output / 'api/axiom.mjs'
            result = subprocess.check_output(['node', '--input-type=module', '-e',
                f"import {{SDK_VERSION,SDK_SEMVER,SDK_BUILD_NUMBER}} from {json.dumps(module.as_uri())};"
                'console.log(JSON.stringify([SDK_VERSION,SDK_SEMVER,SDK_BUILD_NUMBER]))'], text=True)
            self.assertEqual(json.loads(result), [version + '.1234', version, 1234])
            self.assertIn(version + '.1234', (output / 'api/axiom.d.mts').read_text())
            self.assertIn(version + '.1234', (output / 'axiom.d.mts').read_text())
        self.assertEqual(before, {p: (REPO / p).read_bytes() for p in inputs})

    def test_manifest_carries_logical_and_full_identity(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); stage = root / 'stage'; stage.mkdir()
            (stage / 'payload').write_bytes(b'packaging fixture')
            preview_package.package_tree(stage, root / 'sdk.zip', ['payload'], 'a' * 40,
                                         'js-sdk', version='1.3.4', build_number=1234)
            import zipfile
            with zipfile.ZipFile(root / 'sdk.zip') as archive:
                manifest = json.loads(archive.read('manifest.json'))
            self.assertEqual(manifest['sdk_version'], '1.3.4.1234')
            self.assertEqual(manifest['version'], '1.3.4')
            self.assertEqual(manifest['buildNumber'], 1234)

    def test_recompile_requires_a_new_run_but_upload_retry_keeps_identity(self):
        from tools.release.version_identity import ci_build_number, require_fresh_build
        self.assertEqual(ci_build_number({'GITHUB_RUN_NUMBER': '71'}), 71)
        require_fresh_build({'GITHUB_RUN_ATTEMPT': '1'})
        with self.assertRaisesRegex(ValueError, 'new workflow run'):
            require_fresh_build({'GITHUB_RUN_ATTEMPT': '2'})
        with self.assertRaises(ValueError):
            ci_build_number({'GITHUB_RUN_NUMBER': '0'})


if __name__ == '__main__':
    unittest.main()
