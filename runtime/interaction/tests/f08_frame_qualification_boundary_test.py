from pathlib import Path

root = Path(__file__).resolve().parents[3]
runtime = (root / "runtime/interaction/src/canvas_editing_runtime.cpp").read_text(encoding="utf-8")
header = (root / "runtime/interaction/include/canvas/interaction/canvas_editing_runtime.hpp").read_text(encoding="utf-8")
windows = (root / "apps/minimal_runtime_host/windows_main.cpp").read_text(encoding="utf-8")
web = (root / "apps/axiom_canvas_demo/platform/web/bridge.cpp").read_text(encoding="utf-8")
errors = []
present_body = runtime.split("bool CanvasEditingRuntime::presentCanonicalFrame", 1)[1].split(
    "std::optional<render::RuntimeFrameIdentity>", 1)[0]
if "PresentEvidenceKind::kPlatformQualified" in present_body:
    errors.append("presentCanonicalFrame must not synthesize PlatformQualified feedback")
if "RuntimePresentEvidence::kPlatformQualified" in present_body:
    errors.append("presentCanonicalFrame must not synthesize qualified frame-coordinator feedback")
if "receivePlatformPresented" not in header or "receivePlatformPresented" not in runtime:
    errors.append("Runtime must expose an explicit platform-presented feedback ingress")
if "retirePreview" in windows:
    errors.append("Windows host must not decide Runtime preview retirement")
if "PresentEvidenceKind::kPlatformQualified" in web or "receivePlatformPresented" in web:
    errors.append("Web bridge must not synthesize platform-qualified feedback")
if errors:
    print("F08 RED")
    for error in errors:
        print(error)
    raise SystemExit(1)
print("F08 GREEN")
