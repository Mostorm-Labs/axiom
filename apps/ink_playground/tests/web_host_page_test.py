from pathlib import Path


page = Path(__file__).resolve().parents[1] / "platform" / "web" / "index.html"
source = page.read_text(encoding="utf-8")
bridge = page.parent / "bridge.cpp"
bridge_source = bridge.read_text(encoding="utf-8")
assert "let stroke = 0;" in source
assert "stroke += 1;" in source
assert "let sampleSequence = 0;" in source
assert "++sampleSequence" in source
assert "sampleSequence = 0;" not in source.replace("let sampleSequence = 0;", "")
assert "getBoundingClientRect" in source
assert "sample.offsetX" not in source
assert "const strokes = [];" in source
assert "strokes.push(activeStroke);" in source
assert "strokes.forEach" in source
assert "BigInt(" not in source
assert "Cannot mix BigInt" not in source
assert "std::uintptr_t" not in bridge_source
assert "std::size_t" not in bridge_source
assert "std::uint32_t" in bridge_source
