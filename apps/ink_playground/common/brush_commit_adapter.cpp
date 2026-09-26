#include "brush_commit_adapter.hpp"

#include "canvas/foundation/object_id.hpp"

#include <cmath>
#include <string_view>

namespace canvas::ink_playground {
namespace {

canvas::foundation::ObjectId identity(std::string_view value) noexcept {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char byte : value) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  auto result = canvas::foundation::ObjectId::fromUint64(hash == 0U ? 1U : hash);
  result.bytes[8] = 0x42U;
  result.bytes[9] = 0x52U;
  return result;
}

semantic::BrushExecutionSnapshot snapshot(const ink::BrushCommitIntent& intent,
                                          const ink::BrushPackage& package) noexcept {
  semantic::BrushExecutionSnapshot value;
  value.snapshot_version = 2U;
  value.package_revision = package.revision;
  value.pipeline_version = 1U;
  value.defaults_version = 1U;
  value.profile_id = 1U;
  value.signal_schema_version = 1U;
  value.package_id = identity(package.packageId);
  value.vector.size = package.vector.size;
  value.vector.thinning = package.vector.thinning;
  value.vector.smoothing = package.vector.smoothing;
  value.vector.streamline = package.vector.streamline;
  value.vector.pressure_source = static_cast<std::uint32_t>(package.vector.pressureSource);
  value.vector.missing_pressure = static_cast<std::uint32_t>(package.vector.missingPressure);
  value.vector.start_cap = package.vector.startCap;
  value.vector.end_cap = package.vector.endCap;
  value.paint.red = package.paint.red;
  value.paint.green = package.paint.green;
  value.paint.blue = package.paint.blue;
  value.paint.alpha = package.paint.alpha;
  value.paint.opacity = package.paint.opacity;
  value.paint.blend = 1U;
  value.paint.color_space = 1U;
  value.seed = intent.seed;
  value.stages = {
      {1U, static_cast<std::uint32_t>(package.inputMode), 1U, 1U, package.inputMode != ink::BrushStageMode::kOff},
      {2U, static_cast<std::uint32_t>(package.vectorMode), 2U, 1U, package.vectorMode != ink::BrushStageMode::kOff},
      {3U, static_cast<std::uint32_t>(package.renderingMode), 3U, 1U, package.renderingMode != ink::BrushStageMode::kOff},
  };
  return value;
}

}  // namespace

semantic::Operation BrushCommitAdapter::build(
    const ink::BrushCommitIntent& intent,
    const ink::BrushPackage& package,
    std::uint64_t operationId,
    semantic::DocumentId documentId) noexcept {
  semantic::Operation operation;
  operation.id = semantic::OperationId(canvas::foundation::ObjectId::fromUint64(operationId));
  operation.document_id = documentId;
  operation.schema_version = 1U;
  operation.payload_version = 2U;

  semantic::ObjectRecord object;
  object.id = canvas::foundation::ObjectId::fromUint64(operationId);
  object.kind = semantic::ObjectKind::kVectorStroke;
  object.kind_version = 2U;
  object.placement.order_key = semantic::OrderKey(
      {static_cast<std::uint8_t>((operationId % 254U) + 1U)});
  semantic::BrushStrokeContent content;
  content.stroke.snapshot = snapshot(intent, package);
  content.stroke.confirmed_samples.reserve(intent.confirmed.size());
  for (const auto& sample : intent.confirmed) {
    semantic::BrushConfirmedSample confirmed;
    confirmed.position = {sample.x, sample.y};
    if (sample.pressurePresent && std::isfinite(sample.pressure)) {
      confirmed.pressure = sample.pressure;
    }
    content.stroke.confirmed_samples.push_back(confirmed);
  }
  content.stroke.vector_output.fill_rule = 1U;
  content.stroke.vector_output.closed = true;
  content.stroke.vector_output.outline.reserve(intent.outline.size());
  for (const auto& point : intent.outline) {
    content.stroke.vector_output.outline.push_back({point.x, point.y});
  }
  object.content = std::move(content);
  operation.payload = semantic::AddStrokeOp{std::move(object)};
  return operation;
}

bool BrushCommitAdapter::valid(const ink::BrushCommitIntent& intent,
                               const ink::BrushPackage& package) noexcept {
  if (intent.session == 0U || intent.revision == 0U || intent.confirmed.empty() ||
      intent.outline.size() < 3U || package.packageId.empty() || package.revision == 0U) {
    return false;
  }
  for (const auto& sample : intent.confirmed) {
    if (!std::isfinite(sample.x) || !std::isfinite(sample.y) ||
        (sample.pressurePresent && !std::isfinite(sample.pressure))) {
      return false;
    }
  }
  for (const auto& point : intent.outline) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
  }
  return true;
}

}  // namespace canvas::ink_playground
