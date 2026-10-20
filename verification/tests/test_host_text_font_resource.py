"""The shipped font must be reproducible, redistributable and cover Chinese."""
import hashlib
import json
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FONT = ROOT / "assets/fonts/noto-sans-cjk-sc"


def tables(data):
    count = struct.unpack_from(">H", data, 4)[0]
    result = {}
    for i in range(count):
        tag, _, offset, size = struct.unpack_from(">4sIII", data, 12 + i * 16)
        result[tag] = (offset, size)
    return result


def repertoire(data):
    offset, _ = tables(data)[b"cmap"]
    count = struct.unpack_from(">H", data, offset + 2)[0]
    result = set()
    for i in range(count):
        _, _, relative = struct.unpack_from(">HHI", data, offset + 4 + i * 8)
        base = offset + relative
        if struct.unpack_from(">H", data, base)[0] != 12:
            continue
        groups = struct.unpack_from(">I", data, base + 12)[0]
        for j in range(groups):
            first, last, glyph = struct.unpack_from(">III", data, base + 16 + j * 12)
            result.update(range(first + (glyph == 0), last + 1))
    return result


class HostTextFontResource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((FONT / "resource.json").read_text(encoding="utf-8"))
        cls.data = (FONT / cls.manifest["file"]).read_bytes()

    def test_published_bytes_and_exact_static_face(self):
        self.assertEqual(len(self.data), self.manifest["bytes"])
        self.assertEqual(hashlib.sha256(self.data).hexdigest(), self.manifest["sha256"])
        self.assertRegex(self.manifest["upstream_revision"], r"^[0-9a-f]{40}$")
        self.assertEqual(self.manifest["resource_id_uint64"], 0xFC0202)
        self.assertEqual(self.manifest["weight"], 400)
        self.assertFalse(self.manifest["italic"])
        self.assertIn(b"CFF ", tables(self.data))
        self.assertNotIn(b"CFF2", tables(self.data))

    def test_license_and_copyright_accompany_font(self):
        license_text = (FONT / self.manifest["license_file"]).read_text(encoding="utf-8")
        notice = (FONT / self.manifest["notice_file"]).read_text(encoding="utf-8")
        self.assertIn("SIL OPEN FONT LICENSE Version 1.1", license_text)
        self.assertIn("2014-2021 Adobe", notice)
        self.assertIn("Noto is a trademark of Google Inc.", notice)

    def test_actual_cmap_covers_gb2312_and_mixed_text(self):
        points = repertoire(self.data)
        required = set(range(32, 127))
        for first in range(0xA1, 0xF8):
            for last in range(0xA1, 0xFF):
                try:
                    required.add(ord(bytes([first, last]).decode("gb2312")))
                except UnicodeDecodeError:
                    pass
        required.update(map(ord, "你好，世界！Axiom 中文输入测试 123 繁體龍畫筆選取"))
        self.assertEqual(required - points, set(), "published font has missing required glyphs")
        # Retain a genuinely unsupported valid scalar for negative render tests.
        self.assertNotIn(0x10FFFD, points)


class ImportedFontCatalog(unittest.TestCase):
    def test_catalog_bytes_licenses_and_ids_are_exact(self):
        root = ROOT / "assets/fonts/imported"
        catalog = json.loads((root / "catalog.json").read_text(encoding="utf-8"))
        self.assertEqual(len(catalog["fonts"]), 11)
        identities = set()
        families = set()
        for entry in catalog["fonts"]:
            with self.subTest(family=entry["family"]):
                data = (root / entry["file"]).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), entry["sha256"])
                self.assertEqual(len(data), entry["bytes"])
                self.assertIn(b"cmap", tables(data))
                license_text = (root / entry["license_file"]).read_text(encoding="utf-8-sig")
                self.assertIn("LICENSE", license_text.upper())
                self.assertNotIn(entry["resource_id_uint64"], identities)
                self.assertNotIn(entry["family"], families)
                identities.add(entry["resource_id_uint64"])
                families.add(entry["family"])
        self.assertNotIn(0xFC0202, identities)
        self.assertRegex(catalog["source_archive_sha256"], r"^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
