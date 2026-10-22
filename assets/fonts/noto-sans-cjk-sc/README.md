# Host text font

This is the unmodified published **Noto Sans CJK SC Regular 2.004** OpenType font.
It supplies the initial font for new text in Windows/Web Minimal Host. It is an
application ResourceManifest payload, separate from the Skia/Semantic SDKs.

`resource.json` pins the upstream revision, file size and SHA-256. CMake verifies
those bytes and generates the host content-hash constant. Runtime performs its
existing ResourceManifest validation and exact `(weight, italic)` resolution.
No platform fallback or synthetic face is enabled. The new resource ID is
`0xFC0202`; previous Roboto resources retain their own identity and bytes.

Ship `LICENSE.txt` and `NOTICE.txt` alongside the font. The Web package embeds
these files as well as exposing readable copies next to the page. The font is
licensed under SIL Open Font License 1.1; documents created with it retain their
own licenses. This is a fixed Regular face, not a claim to support every weight,
italic face, Unicode scalar or emoji.

Upstream: https://github.com/notofonts/noto-cjk/tree/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans
