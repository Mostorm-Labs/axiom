from pathlib import Path
root=Path(__file__).parents[2]
facade=(root/"runtime/foundation/include/canvas/runtime/runtime_facade.hpp").read_text()
assert "ImageImport" not in facade
assert "Crop" not in facade
print("FC03-RED-01 PASS: old facade has no typed image import/crop API")
