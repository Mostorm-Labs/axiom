# Chalk revision 3 screenshot-derived resources

These PNGs are deterministic 1024×1024 editor-canvas crops, downsampled to
256×256, extracted from the user-provided Grain Editor and Shape Editor
screenshots. The `.brush` archive contained references to the shape and grain
JPG names but did not embed those source JPG bytes, and the original files
were not available in the workspace.

They are therefore **screenshot-extracted provisional resources**, not claims
of the original Procreate assets:

| Resource | File | SHA-256 |
| --- | --- | --- |
| shape | `shape-screenshot-extract-v1.png` | `4171df4f399e11b7bc44fa9c688c19f7517622b95c845d63eadda91bbd8feaae` |
| grain | `grain-screenshot-extract-v1.png` | `d6c0834bd8e9992c3ca448ca5747b76db4ec7f79cd3d82167273c7d71e1bd520` |

The runtime package binds both resources by ID, version, kind, and SHA-256.

Revision 4 sources (current Web default):

- shape source: `shape-editor-source-v2.png`, derived `shape-screenshot-extract-v2.png`, SHA-256 `d88a0c58d904a5041bed7f1d79273f4dadcbd92b31e0bb76a0c5dc376ecf594a`
- grain source: `grain-editor-source-v2.png`, derived `grain-screenshot-extract-v2.png`, SHA-256 `b0770b02eaaf52869ea4dbe16b034c32d59149fbece1a9abf21fec55712a8fe6`

The renderer uses revision 4 continuous stroke coverage with canvas-space grain.
It does not submit one independent textured stamp per dab.

Source screenshots:

- `/Users/qing/Downloads/IMG_5695.PNG` (Grain Editor)
- `/Users/qing/Downloads/IMG_5696.PNG` (Shape Editor)

Crop: top-left `(x=1007, y=77)`, size `1024×1024`, then deterministic
downsample to `256×256` with macOS `sips`.
