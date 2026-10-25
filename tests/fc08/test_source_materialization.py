"""Acceptance checks for isolated M01 artifacts, never for product readiness."""
from pathlib import Path
import hashlib
import json
import unittest

from verification.tools.descriptor_refreeze_diff import parse_descriptor

ROOT = Path(__file__).resolve().parents[2]
MAT = ROOT / '.aegis/materialized/FC-08'


class SourceMaterialization(unittest.TestCase):
    def test_complete_source_bound_descriptors_preserve_inherited_fields(self):
        self.assertTrue((MAT / 'baseline-descriptor/descriptor.pb').is_file(),
                        'M01 BASE-01 descriptor has not been generated')
        base = parse_descriptor((MAT / 'baseline-descriptor/descriptor.pb').read_bytes())
        proposed = parse_descriptor((MAT / 'proposed-descriptor/descriptor.pb').read_bytes())
        for name, f in base.items():
            self.assertEqual(f.enums, proposed[name].enums)
            messages = {m.name: m for m in proposed[name].messages}
            for m in f.messages:
                for field in m.fields:
                    self.assertIn(field, messages[m.name].fields)
        def fields(tree, filename, msg):
            return {f.number: f.name for m in tree['auditoryworks/axiom/v1/' + filename].messages
                    if m.name == msg for f in m.fields}
        self.assertEqual(set(fields(base, 'operation.proto', 'OperationPayload')), set(range(1,17)))
        self.assertEqual(fields(base, 'object.proto', 'ShapeContent')[4], 'label')
        self.assertEqual(fields(base, 'object.proto', 'ObjectContent')[10], 'brush_stroke')
        self.assertEqual(fields(base, 'object.proto', 'ObjectContent')[11], 'dab_brush_stroke')
        self.assertEqual(fields(proposed, 'object.proto', 'ObjectContent')[12], 'section')
        self.assertEqual(fields(proposed, 'operation.proto', 'OperationPayload')[17], 'set_section_content')
        self.assertEqual(fields(proposed, 'operation.proto', 'OperationPayload')[18], 'edit_container_topology')

    def test_exports_have_exact_hashes_and_acceptance_events(self):
        self.assertTrue((MAT / 'source-index.json').is_file(), 'M01 source index missing')
        index = json.loads((MAT / 'source-index.json').read_text(encoding='utf-8'))
        self.assertEqual({s['id'] for s in index['accepted_exports']},
                         {'P12','P13','P14','P15','P16','P17','P20 execution'})
        for item in index['accepted_exports']:
            data = (ROOT / item['path']).read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(), item['sha256'])
            self.assertTrue(item['acceptance_event'])
            self.assertNotIn(b'<ancestor-path>', data)
            self.assertNotIn(b'Here is the result of', data)


if __name__ == '__main__':
    unittest.main()
