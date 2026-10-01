# Procreate brushset parsing

`membrane_brushset_parser.py` is a read-only parser for the legacy Procreate
`.brushset` format. It follows the same archive/resource layout used by the
MIT-licensed [Spectator](https://github.com/keimbio/Spectator) project:

- decode `brushset.plist` and each `Brush.archive` NSKeyedArchive;
- preserve the decoded parameter tree as JSON data;
- extract the sibling `Shape.png`, `Grain.png`, and QuickLook thumbnail;
- record dimensions and SHA-256 provenance for every extracted resource.

Example:

```bash
python3 -m tools.brush.membrane_brushset_parser \
  /path/to/Procreate_1.brushset \
  --name Membrane \
  --report membrane-brushset-report.json
```

The parser does not emulate Procreate rendering. `bundledShapePath` and
`bundledGrainPath` may legitimately decode as `null`; in that case the sibling
PNG files in the brushset directory are the resource payloads. The QuickLook
thumbnail is retained as a visual oracle, not used as runtime geometry.
