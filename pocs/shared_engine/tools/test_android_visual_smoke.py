#!/usr/bin/env python3
"""Tests for the independent Android POC-01 visual smoke result contract."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from run_android_visual_smoke import (  # noqa: E402
    TEXT_REGION,
    analyze_text_region,
    compare_visual_gate,
)


class AndroidVisualSmokeTest(unittest.TestCase):
    def test_pinned_fixture_contains_canvas_v2_text_pixels(self) -> None:
        rgba = (ROOT / "goldens/reference.rgba").read_bytes()
        metrics = analyze_text_region(rgba)
        self.assertTrue(metrics["non_background_pixels"] > 0)
        self.assertEqual(metrics["region"], list(TEXT_REGION))

    def test_blank_text_region_is_rejected(self) -> None:
        rgba = bytearray((ROOT / "goldens/reference.rgba").read_bytes())
        x0, y0, x1, y1 = TEXT_REGION
        for y in range(y0, y1):
            for x in range(x0, x1):
                offset = (y * 800 + x) * 4
                rgba[offset : offset + 4] = bytes((244, 245, 247, 255))
        metrics = analyze_text_region(bytes(rgba))
        self.assertEqual(metrics["non_background_pixels"], 0)
        self.assertFalse(metrics["passed"])

    def test_visual_gate_requires_shape_and_ratio(self) -> None:
        expected = (ROOT / "goldens/reference.rgba").read_bytes()
        report = compare_visual_gate(expected, expected)
        self.assertTrue(report["passed"])
        self.assertEqual(report["matching_ratio"], 1.0)
        with self.assertRaises(ValueError):
            compare_visual_gate(expected, expected[:-4])


if __name__ == "__main__":
    unittest.main()
