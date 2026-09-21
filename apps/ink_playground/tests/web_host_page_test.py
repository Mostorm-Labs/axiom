from pathlib import Path


page = Path(__file__).resolve().parents[1] / "platform" / "web" / "index.html"
source = page.read_text(encoding="utf-8")
bridge = page.parent / "bridge.cpp"
bridge_source = bridge.read_text(encoding="utf-8")
worker = page.parent / "evidence_worker.js"
worker_source = worker.read_text(encoding="utf-8")
assert "let stroke = 0;" in source
assert "stroke += 1;" in source
assert "let sampleSequence = 0;" in source
assert "++sampleSequence" in source
assert "sampleSequence = 0;" not in source.replace("let sampleSequence = 0;", "")
assert "getBoundingClientRect" in source
assert "sample.offsetX" not in source
assert "const strokes = [];" in source
assert "strokes.push({ points: activeStroke.points, family: activeStroke.family, runtime: runtimeBrush.get(event.pointerId) });" in source
assert "committed.forEach(stroke => drawBrushSegment" in source
assert "BigInt(" not in source
assert "const trace = [];" in source
assert "scheduleEvidenceSnapshot" in source
assert "setTimeout(() => persistEvidence" in source
assert "convertToBlob" in worker_source
assert "crypto.subtle.digest" in worker_source
assert "exportEvidence" in source
assert "_axiom_ink_cancel_pointer" in source
assert "axiom_ink_cancel_pointer" in bridge_source
assert "_axiom_ink_cancel_pointer" in (page.parents[2] / "CMakeLists.txt").read_text()
assert "_axiom_ink_viewport_claimed" in source
assert "axiom_ink_viewport_claimed" in bridge_source
assert "touch-action: none" in source
assert "preventDefault" in source
assert "gesturechange" in source
assert "wheel" in source
assert "handleWheel" in source
assert "id=\"brushSelector\"" in source
assert "selectedBrushFamily" in source
assert "brushFamilyName" in source
assert "family: selectedBrushFamily" in source
assert "drawBrushSegment" in source
assert "_axiom_ink_brush_begin" in source
assert "_axiom_ink_brush_sample" in source
assert "_axiom_ink_brush_size" in source
assert "_axiom_ink_brush_representation" in source
assert "axiom_ink_brush_begin" in bridge_source
assert "axiom_ink_brush_sample" in bridge_source
assert "addEventListener(\"wheel\", handleWheel, { passive: false, capture: true })" in source
assert "trackpadTranslationX -= event.deltaX" in source
assert "trackpadTranslationY -= event.deltaY" in source
assert "viewportScale" in source
assert "viewportTranslationX" in source
assert "viewportTranslationY" in source
assert "viewportTranslationX: viewportTranslationX().toFixed(2)" in source
assert "viewportTranslationY: viewportTranslationY().toFixed(2)" in source
assert "coalesced.length === 0" in source
assert "_axiom_ink_pointer_sample_phase" in source
assert "_axiom_ink_viewport_scale" in source
assert "trace_sha256" in worker_source
assert "Cannot mix BigInt" not in source
assert "std::uintptr_t" not in bridge_source
assert "std::size_t" not in bridge_source
assert "std::uint32_t" in bridge_source
