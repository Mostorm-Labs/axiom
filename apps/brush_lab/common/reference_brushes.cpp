#include "reference_brushes.hpp"

#include "canvas/ink/reference_brush_catalog.hpp"

namespace canvas::brush_lab {
ReferenceBrushSet makeReferenceBrushSet() {
  return ink::makeReferenceBrushCatalog();
}

std::string referenceBrushManifestJson() {
  return ink::referenceBrushCatalogManifestJson();
}

ink::BrushCompileResult compileQualificationBrush(
    const ReferenceBrushPreset& preset, const ink::ResourceCatalog& resources) {
  return ink::compileReferenceBrush(preset, resources);
}

ReferenceBrushPreset editDryChalk(const ReferenceBrushPreset& preset,
                                  DryChalkEdit edit) {
  auto out = preset;
  out.definition.nominalSize = edit.nominalSize;
  out.definition.spacing = edit.spacing;
  out.definition.opacity = edit.opacity;
  return out;
}

QualificationStroke runReferenceBrushStroke(
    const ReferenceBrushPreset& preset, const ink::ResourceCatalog& resources,
    std::span<const ink::BrushInputSample> samples, std::uint64_t sessionId) {
  QualificationStroke out;
  if (samples.empty() || sessionId == 0U) return out;
  const auto compiled = compileQualificationBrush(preset, resources);
  if (!compiled) return out;
  ink::BrushRuntime runtime(resources);
  const ink::BrushSessionId session{sessionId};
  if (!runtime.begin(session, *compiled.program, 0x4A450000ULL + sessionId)) return out;
  const auto appended = runtime.append(session, samples);
  if (!appended) {
    (void)runtime.cancel(session);
    return out;
  }
  out.preview = appended.preview.primitives;
  out.commit = runtime.finish(session).commit;
  return out;
}

ReferenceBrushStrokeSession::ReferenceBrushStrokeSession(
    const ReferenceBrushPreset& preset, const ink::ResourceCatalog& resources,
    std::uint64_t sessionId)
    : runtime_(resources), session_{sessionId} {
  const auto compiled = compileQualificationBrush(preset, resources);
  if (compiled && sessionId != 0U &&
      runtime_.begin(session_, *compiled.program, 0x4A450000ULL + sessionId)) {
    program_ = compiled.program;
    active_ = true;
  }
}

ink::BrushRuntimeResult ReferenceBrushStrokeSession::append(
    std::span<const ink::BrushInputSample> samples) {
  if (!active_) return {};
  return runtime_.append(session_, samples);
}

ink::BrushRuntimeResult ReferenceBrushStrokeSession::finish() {
  if (!active_) return {};
  active_ = false;
  return runtime_.finish(session_);
}

void ReferenceBrushStrokeSession::cancel() noexcept {
  if (active_) (void)runtime_.cancel(session_);
  active_ = false;
}

}  // namespace canvas::brush_lab
