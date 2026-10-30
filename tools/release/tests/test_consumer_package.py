from pathlib import Path
import tempfile
import unittest
from tools.release.consumer_package import validate_native_payload, verify_release
from tools.release.preview_package import package_tree
import json

class ConsumerPayloadTest(unittest.TestCase):
    def test_rejects_internal_static_archives(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'runtime.lib').write_bytes(b'internal')
            with self.assertRaisesRegex(ValueError,'internal'):
                validate_native_payload(p)

    def test_requires_licenses_and_runtime_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            for name in ('bin/axiom.dll','lib/axiom.lib','include/axiom/api.h','include/axiom/axiom.hpp','lib/cmake/AxiomSDK/AxiomSDKConfig.cmake'):
                q=p/name;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(b'consumer')
            with self.assertRaisesRegex(ValueError,'missing'):
                validate_native_payload(p)

    def test_same_revision_is_insufficient_when_platform_versions_differ(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);release=root/'release';release.mkdir()
            for name,kind in {'axiom-sdk-windows-preview.zip':'windows-sdk','axiom-sdk-js-preview.zip':'js-sdk','axiom-demo-windows-preview.zip':'windows-demo','axiom-demo-web-preview.zip':'web-demo'}.items():
                stage=root/kind;stage.mkdir();(stage/'payload').write_bytes(b'packaging fixture, not product evidence')
                if kind=='windows-sdk':
                    (stage/'lib').mkdir();(stage/'lib/axiom.lib').write_bytes(b'import stub fixture')
                if kind=='js-sdk':(stage/'package.json').write_text(json.dumps({'version':'1.0.0-preview.1'}))
                package_tree(stage,release/name,['payload'],'d'*40,kind,version='1.0.0-preview.2' if kind=='web-demo' else '1.0.0-preview.1')
            with self.assertRaisesRegex(ValueError,'versions differ'):
                verify_release(release,'d'*40)
            self.assertFalse((release/'SHA256SUMS').exists())

if __name__=='__main__':unittest.main()
