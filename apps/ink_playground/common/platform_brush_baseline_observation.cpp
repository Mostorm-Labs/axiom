#include "platform_brush_baseline_observation.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace canvas::ink_playground {
namespace {
const char* phaseName(input::PointerPhase phase) {
  switch (phase) {
    case input::PointerPhase::kDown: return "down";
    case input::PointerPhase::kMove: return "move";
    case input::PointerPhase::kUp: return "up";
    case input::PointerPhase::kCancel: return "cancel";
  }
  return "unknown";
}
const char* dispositionName(interaction::ContactDisposition disposition) {
  switch (disposition) {
    case interaction::ContactDisposition::kPending: return "pending";
    case interaction::ContactDisposition::kInk: return "ink";
    case interaction::ContactDisposition::kViewportGesture: return "viewport";
    case interaction::ContactDisposition::kIgnored: return "ignored";
    case interaction::ContactDisposition::kTerminal: return "terminal";
  }
  return "unknown";
}
const char* sourceName(input::InputSourceId source) {
  return source == 1 ? "ink" : source == 11 ? "touch-a" : source == 12 ? "touch-b" : "unknown";
}
std::string escape(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const char character : value) {
    if (character == '\\' || character == '"') result.push_back('\\');
    result.push_back(character);
  }
  return result;
}
template <typename T>
void comma(std::ostringstream& out, std::size_t index, T&& value) {
  if (index != 0) out << ',';
  out << value;
}
bool matchesFixture(const InkPlaygroundHost& host) {
  struct Expected final { input::InputSourceId source; input::PointerId pointer;
    std::uint64_t sequence; float x; float y; float contentX; float contentY;
    float pressure; input::PointerPhase phase; };
  constexpr Expected expected[] = {
      {1, 1, 1, 10, 20, 10, 20, .5F, input::PointerPhase::kDown},
      {1, 1, 2, 18, 28, 18, 28, .6F, input::PointerPhase::kMove},
      {1, 1, 3, 30, 40, 30, 40, .7F, input::PointerPhase::kMove},
      {1, 1, 4, 42, 52, 42, 52, .8F, input::PointerPhase::kUp},
      {11, 21, 5, 0, 0, 0, 0, 0, input::PointerPhase::kDown},
      {12, 22, 6, 20, 0, 20, 0, 0, input::PointerPhase::kDown},
      {12, 22, 7, 40, 0, 20, 0, 0, input::PointerPhase::kMove},
      {11, 21, 8, 0, 0, 0, 0, 0, input::PointerPhase::kUp},
      {12, 22, 9, 40, 0, 20, 0, 0, input::PointerPhase::kUp},
  };
  const auto& trace = host.baselineTrace();
  if (trace.size() != std::size(expected) || host.baselineViewport().size() != trace.size() ||
      host.brushResolvedStateDigest() == 0 || host.brushPreviewDigest() == 0 ||
      host.brushSealedOutlineDigest() == 0 ||
      host.brushSealedOutlineDigest() != host.brushReplayDigest()) {
    return false;
  }
  for (std::size_t i = 0; i < trace.size(); ++i) {
    const auto near = [](float left, float right) { return std::abs(left - right) < .0005F; };
    if (trace[i].key.source != expected[i].source || trace[i].key.pointer != expected[i].pointer ||
        trace[i].key.generation != 1U ||
        trace[i].sequence != expected[i].sequence || trace[i].phase != expected[i].phase ||
        !near(trace[i].viewX, expected[i].x) || !near(trace[i].viewY, expected[i].y) ||
        !near(trace[i].contentX, expected[i].contentX) ||
        !near(trace[i].contentY, expected[i].contentY) ||
        !near(trace[i].pressure, expected[i].pressure)) return false;
    const bool claimed = i >= 5U && i <= 7U;
    const float scale = i >= 6U ? 2.0F : 1.0F;
    const auto& viewport = host.baselineViewport()[i];
    if (viewport.sequence != expected[i].sequence || viewport.claimed != claimed ||
        !near(viewport.scale, scale) || !near(viewport.translationX, 0.0F) ||
        !near(viewport.translationY, 0.0F)) return false;
  }
  return true;
}
}  // namespace

bool runPlatformBrushBaselineFixture(InkPlaygroundHost& host) {
  struct FixtureSample final {
    std::uint64_t source, pointer, sequence, timestamp;
    float x, y, pressure;
    input::PointerPhase phase;
  };
  constexpr FixtureSample samples[] = {
      {1, 1, 1, 1000000, 10, 20, .5F, input::PointerPhase::kDown},
      {1, 1, 2, 2000000, 18, 28, .6F, input::PointerPhase::kMove},
      {1, 1, 3, 3000000, 30, 40, .7F, input::PointerPhase::kMove},
      {1, 1, 4, 4000000, 42, 52, .8F, input::PointerPhase::kUp},
      {11, 21, 5, 5000000, 0, 0, 0, input::PointerPhase::kDown},
      {12, 22, 6, 6000000, 20, 0, 0, input::PointerPhase::kDown},
      {12, 22, 7, 7000000, 40, 0, 0, input::PointerPhase::kMove},
      {11, 21, 8, 8000000, 0, 0, 0, input::PointerPhase::kUp},
      {12, 22, 9, 9000000, 40, 0, 0, input::PointerPhase::kUp},
  };
  if (!host.bindSurface(1024, 768)) return false;
  for (const auto& value : samples) {
    input::PlatformPointerBatch batch;
    batch.samples.push_back({value.source, value.pointer, value.sequence, value.timestamp,
        value.x, value.y, value.pressure, 0.0F, 0.0F, {}, {},
        input::SampleProvenance::kConfirmedCurrent, value.phase});
    if (!host.acceptPlatformBatch(batch, value.timestamp)) return false;
  }
  return matchesFixture(host);
}

std::string platformBrushBaselineObservationJson(
    std::string_view platform, std::string_view reality, std::string_view artifactIdentity,
    const InkPlaygroundHost& host, const BaselineRenderPath& renderPath) {
  auto observed = renderPath;
  observed.canonicalSurfaceGeneration = host.surface().generation;
  observed.previewSurfaceGeneration = host.previewSurfaceGeneration();
  const auto& previewState = host.previewRenderState();
  observed.previewContentRevision = previewState.contentRevision;
  observed.previewSubmittedRevision = previewState.submittedRevision;
  observed.previewDirty = previewState.dirty;
  observed.previewSubmissionCount = previewState.submissionCount;
  observed.previewPresentCount = previewState.presentCount;
  const auto& trace = host.baselineTrace();
  const auto& viewport = host.baselineViewport();
  const bool complete = matchesFixture(host) && !artifactIdentity.empty();
  const bool finding = observed.readbackCount != 0 || observed.cpuCopyCount != 0 ||
                       observed.viewportTransformApplied != "true";
  const std::string_view classification = complete
      ? (finding ? "PASS_WITH_FINDINGS" : "PASS") : "BLOCKED_EVIDENCE";
  std::ostringstream out;
  out << std::setprecision(9)
      << "{\n  \"schema_version\":\"platform-brush-baseline-v1\",\n"
      << "  \"fixture_digest\":\"" << kPlatformBrushBaselineFixtureDigest << "\",\n"
      << "  \"platform\":\"" << escape(platform) << "\",\n"
      << "  \"reality\":\"" << escape(reality) << "\",\n"
      << "  \"artifact_identity\":\"" << escape(artifactIdentity) << "\",\n"
      << "  \"runtime_identity\":{\"semantic_generation\":"
      << host.semanticGeneration().value() << ",\"scene_revision\":"
      << host.sceneRevision() << ",\"surface_generation\":"
      << host.surface().generation << ",\"canonical_commit_ordinal\":"
      << host.canonicalCommitOrdinal() << "},\n"
      << "  \"input\":{\n    \"normalized_samples\":[";
  for (std::size_t i = 0; i < trace.size(); ++i) {
    comma(out, i, "{\"source\":\"" + std::string(sourceName(trace[i].key.source)) +
        "\",\"pointer\":" + std::to_string(trace[i].key.pointer) +
        ",\"generation\":" + std::to_string(trace[i].key.generation) +
        ",\"sequence\":" + std::to_string(trace[i].sequence) +
        ",\"x\":" + std::to_string(trace[i].contentX) +
        ",\"y\":" + std::to_string(trace[i].contentY) +
        ",\"pressure\":" + std::to_string(trace[i].pressure) +
        ",\"phase\":\"" + phaseName(trace[i].phase) + "\"" +
        ",\"disposition\":\"" + dispositionName(trace[i].disposition) + "\"" +
        ",\"viewport_claimed\":" +
        (trace[i].viewportClaimed ? "true" : "false") +
        ",\"down_timestamp_ns\":" + std::to_string(trace[i].downTimestampNs) +
        ",\"down_time_delta_ns\":" + std::to_string(trace[i].downTimeDeltaNs) + "}");
  }
  out << "],\n    \"pointer_identity\":[";
  for (std::size_t i = 0; i < trace.size(); ++i) comma(out, i,
      "{\"source\":\"" + std::string(sourceName(trace[i].key.source)) +
      "\",\"pointer\":" + std::to_string(trace[i].key.pointer) +
      ",\"generation\":" + std::to_string(trace[i].key.generation) + "}");
  out << "],\n    \"phase_lifecycle\":[";
  for (std::size_t i = 0; i < trace.size(); ++i) comma(out, i, std::string("\"") + phaseName(trace[i].phase) + "\"");
  out << "],\n    \"viewport_claim\":[";
  for (std::size_t i = 0; i < viewport.size(); ++i) comma(out, i,
      "{\"sequence\":" + std::to_string(viewport[i].sequence) +
      ",\"claimed\":" + std::string(viewport[i].claimed ? "true" : "false") +
      ",\"scale\":" + std::to_string(viewport[i].scale) +
      ",\"translation_x\":" + std::to_string(viewport[i].translationX) +
      ",\"translation_y\":" + std::to_string(viewport[i].translationY) + "}");
  out << "],\n    \"content_coordinates\":[";
  for (std::size_t i = 0; i < trace.size(); ++i) comma(out, i,
      "{\"sequence\":" + std::to_string(trace[i].sequence) +
      ",\"x\":" + std::to_string(trace[i].contentX) +
      ",\"y\":" + std::to_string(trace[i].contentY) + "}");
  out << "],\n    \"interaction_decisions\":[";
  for (std::size_t i = 0; i < trace.size(); ++i) comma(out, i,
      "{\"sequence\":" + std::to_string(trace[i].sequence) +
      ",\"pointer\":" + std::to_string(trace[i].key.pointer) +
      ",\"generation\":" + std::to_string(trace[i].key.generation) +
      ",\"phase\":\"" + phaseName(trace[i].phase) + "\"" +
      ",\"disposition\":\"" + dispositionName(trace[i].disposition) + "\"" +
      ",\"viewport_claimed\":" +
      (trace[i].viewportClaimed ? "true" : "false") +
      ",\"down_timestamp_ns\":" + std::to_string(trace[i].downTimestampNs) +
      ",\"down_time_delta_ns\":" + std::to_string(trace[i].downTimeDeltaNs) + "}");
  out << "]},\n  \"brush\":{\"profile_id\":\"vector-solid-v1\",\"package_id\":\"00000000000000000000000000000042\",\"package_revision\":1,\"package_digest\":\"" << host.brushPackageDigest() << "\""
      << ",\"resolved_state_digest\":\"" << host.brushResolvedStateDigest()
      << "\",\"preview_outline_digest\":\"" << host.brushPreviewDigest()
      << "\",\"sealed_outline_digest\":\"" << host.brushSealedOutlineDigest()
      << "\",\"replay_digest\":\"" << host.brushReplayDigest() << "\"},\n"
      << "  \"render_path\":{\"input_boundary\":\"" << escape(renderPath.inputBoundary)
      << "\",\"runtime_boundary\":\"" << escape(renderPath.runtimeBoundary)
      << "\",\"renderer\":\"" << escape(renderPath.renderer)
      << "\",\"surface_type\":\"" << escape(renderPath.surfaceType)
      << "\",\"submission_count\":" << renderPath.submissionCount
      << ",\"readback_count\":" << renderPath.readbackCount
      << ",\"cpu_copy_count\":" << renderPath.cpuCopyCount
      << ",\"present_count\":" << renderPath.presentCount
      << ",\"viewport_transform_applied\":\"" << escape(renderPath.viewportTransformApplied)
      << "\"},\n  \"preview_surface\":{\"canonical_surface_generation\":"
      << observed.canonicalSurfaceGeneration << ",\"preview_surface_generation\":"
      << observed.previewSurfaceGeneration << ",\"preview_content_revision\":"
      << observed.previewContentRevision << ",\"preview_submitted_revision\":"
      << observed.previewSubmittedRevision << ",\"preview_dirty\":"
      << (observed.previewDirty ? "true" : "false") << ",\"preview_submission_count\":"
      << observed.previewSubmissionCount << ",\"preview_present_count\":"
      << observed.previewPresentCount << ",\"geometry_source\":\"BrushPreviewDelta.outline\","
      << "\"preview_style\":{\"color\":[1.0,0.85,0.0,0.55],\"blend\":\"src_over\"},\"resize_events\":"
      << host.resizeEventCount() << ",\"surface_lost_events\":" << host.surfaceLostCount()
      << ",\"rebind_events\":" << host.rebindEventCount() << "},\n"
      << "  \"classification\":\"" << escape(classification) << "\",\n  \"diagnostics\":[";
  if (!complete) out << "\"Observation is not the complete fixed platform-brush-baseline-v1 corpus or lacks artifact identity.\"";
  out << "]\n}\n";
  return out.str();
}
}  // namespace canvas::ink_playground
