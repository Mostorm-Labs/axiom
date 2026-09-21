#pragma once

#include "canvas/ink/reference_brush_catalog.hpp"

#include <vector>
#include <string>
#include <span>
#include <memory>

namespace canvas::brush_lab {

using ReferenceBrushId = ink::ReferenceBrushId;
using ReferenceBrushPreset = ink::ReferenceBrushPreset;
using ReferenceBrushSet = ink::ReferenceBrushCatalog;

struct DryChalkEdit final {
  float nominalSize = 0.0F;
  float spacing = 0.0F;
  float opacity = 0.0F;
};

struct QualificationStroke final {
  std::vector<ink::BrushPrimitive> preview;
  ink::BrushCommitArtifact commit;
};

class ReferenceBrushStrokeSession final {
 public:
  ReferenceBrushStrokeSession(const ReferenceBrushPreset& preset,
                              const ink::ResourceCatalog& resources,
                              std::uint64_t sessionId);
  [[nodiscard]] bool valid() const noexcept { return active_; }
  [[nodiscard]] ink::BrushRuntimeResult append(
      std::span<const ink::BrushInputSample> samples);
  [[nodiscard]] ink::BrushRuntimeResult finish();
  void cancel() noexcept;

 private:
  std::shared_ptr<const ink::BrushProgram> program_;
  ink::BrushRuntime runtime_;
  ink::BrushSessionId session_{};
  bool active_ = false;
};

[[nodiscard]] ReferenceBrushSet makeReferenceBrushSet();
[[nodiscard]] std::string referenceBrushManifestJson();
[[nodiscard]] ink::BrushCompileResult compileQualificationBrush(
    const ReferenceBrushPreset& preset, const ink::ResourceCatalog& resources);
[[nodiscard]] ReferenceBrushPreset editDryChalk(const ReferenceBrushPreset& preset,
                                                DryChalkEdit edit);
[[nodiscard]] QualificationStroke runReferenceBrushStroke(
    const ReferenceBrushPreset& preset, const ink::ResourceCatalog& resources,
    std::span<const ink::BrushInputSample> samples, std::uint64_t sessionId);

}  // namespace canvas::brush_lab
