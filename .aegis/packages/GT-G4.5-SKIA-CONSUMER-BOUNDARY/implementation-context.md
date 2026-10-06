# GT-G4.5-SKIA-CONSUMER-BOUNDARY

This package repairs only the CMake source-selection boundary exposed by the Windows Semantic SDK consumer. The generic Debug UI library remains available without Skia; Windows host and Win32 backend sources are selected only when the CanvasSkia target exists. The real Skia-enabled Windows path is preserved.
