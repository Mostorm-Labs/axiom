#include "../platform/windows/windows_pointer_utils.hpp"
#include "../platform/windows/windows_smoke_evidence.hpp"
#include "arc/protocol.h"
#include "canvas/ink/ink_engine.hpp"
#include "canvas/interaction/multi_contact_coordinator.hpp"

#if defined(_WIN32)

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

int main() {
  const auto value = canvas::ink_playground::windows_input::deviceIdFromHandle(
      reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(0x1234U)));
  if (value != 0x1234U ||
      canvas::ink_playground::windows_input::deviceIdFromHandle(nullptr) != 0U ||
      canvas::ink_playground::windows_input::functionalDeviceId(nullptr, 7U) == 0U) {
    return 1;
  }
  std::vector<canvas::ink::StrokePoint> retained{{1.0F, 2.0F, 0.5F}};
  canvas::ink_playground::windows_input::appendCommittedStroke(
      retained, {{3.0F, 4.0F, 0.75F}});
  if (retained.size() != 2U || retained.back().x != 3.0F || retained.back().pressure != 0.75F) {
    return 3;
  }
  std::vector<arc_preview_primitive_v0> previewPoints;
  std::uint64_t lastPreviewSequence = 0;
  arc_pointer_sample_v0 previewSamples[2]{};
  previewSamples[0].sample_sequence = 1;
  previewSamples[0].x = 10.0F;
  previewSamples[0].y = 11.0F;
  previewSamples[1].sample_sequence = 2;
  previewSamples[1].x = 12.0F;
  previewSamples[1].y = 13.0F;
  if (canvas::ink_playground::windows_input::appendPreviewSamples(
          previewPoints, lastPreviewSequence, previewSamples) != 2U ||
      canvas::ink_playground::windows_input::appendPreviewSamples(
          previewPoints, lastPreviewSequence, previewSamples) != 0U ||
      previewPoints.size() != 2U || lastPreviewSequence != 2U) {
    return 13;
  }
  const std::vector<canvas::ink::BrushInputSample> vectorSamples{
      {10.0F, 10.0F, 0.2F, 0.0F, 0.0F, 1U},
      {20.0F, 10.0F, 1.0F, 0.0F, 0.0F, 2U},
      {30.0F, 10.0F, 1.0F, 0.0F, 0.0F, 3U}};
  const auto vectorGeometry = canvas::ink::generateVectorStroke(
      vectorSamples, {.size = 10.0F, .thinning = 0.5F, .smoothing = 0.0F});
  const auto arcPoints =
      canvas::ink_playground::windows_input::vectorGeometryToArcPoints(vectorGeometry);
  if (arcPoints.size() != vectorSamples.size() ||
      arcPoints.front().kind != ARC_PREVIEW_PRIMITIVE_VECTOR_POINT ||
      arcPoints[1].radius <= arcPoints.front().radius) {
    return 14;
  }
  std::vector<POINTER_INFO> newestFirst(3);
  newestFirst[0].pointerFlags = POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT;
  newestFirst[0].ptPixelLocation = {100, 100};
  newestFirst[1].pointerFlags = POINTER_FLAG_INRANGE;
  newestFirst[1].ptPixelLocation = {80, 80};
  newestFirst[2].pointerFlags = POINTER_FLAG_INRANGE;
  newestFirst[2].ptPixelLocation = {60, 60};
  const auto downHistory =
      canvas::ink_playground::windows_input::normalizePointerHistory(newestFirst, true);
  if (downHistory.size() != 1U || downHistory.front().ptPixelLocation.x != 100) return 4;

  newestFirst[0].pointerFlags = POINTER_FLAG_INCONTACT;
  newestFirst[0].ptPixelLocation = {120, 120};
  newestFirst[1].pointerFlags = POINTER_FLAG_INCONTACT;
  newestFirst[1].ptPixelLocation = {110, 110};
  const auto moveHistory =
      canvas::ink_playground::windows_input::normalizePointerHistory(newestFirst, false);
  if (moveHistory.size() != 2U || moveHistory.front().ptPixelLocation.x != 110 ||
      moveHistory.back().ptPixelLocation.x != 120) return 5;

  using canvas::ink_playground::windows_input::PointerLifecycle;
  using canvas::ink_playground::windows_input::PointerLifecycleEvent;
  PointerLifecycle lifecycle;
  if (lifecycle.update(1U) != PointerLifecycleEvent::kIgnore ||
      lifecycle.end(1U) != PointerLifecycleEvent::kIgnore ||
      lifecycle.begin(1U, 100U) != PointerLifecycleEvent::kBegin ||
      lifecycle.update(2U) != PointerLifecycleEvent::kIgnore ||
      lifecycle.update(1U) != PointerLifecycleEvent::kUpdate ||
      lifecycle.end(1U) != PointerLifecycleEvent::kEnd ||
      lifecycle.update(1U) != PointerLifecycleEvent::kIgnore ||
      lifecycle.end(1U) != PointerLifecycleEvent::kIgnore ||
      lifecycle.begin(1U, 200U) != PointerLifecycleEvent::kBegin ||
      lifecycle.strokeStartTime() != 200U) {
    return 6;
  }

  newestFirst.resize(2);
  newestFirst[0].pointerFlags = POINTER_FLAG_INCONTACT;
  newestFirst[0].dwTime = 210U;
  newestFirst[1].pointerFlags = POINTER_FLAG_INCONTACT;
  newestFirst[1].dwTime = 190U;
  const auto currentStrokeHistory =
      canvas::ink_playground::windows_input::normalizePointerHistory(newestFirst, false, 200U);
  if (currentStrokeHistory.size() != 1U || currentStrokeHistory.front().dwTime != 210U) return 7;
  using canvas::ink_playground::windows_input::PointerEvidenceSample;
  using canvas::ink_playground::windows_input::serializePointerTrace;
  if (canvas::ink_playground::windows_input::arcToolForPointerType(PT_TOUCH) !=
          ARC_INPUT_TOOL_TOUCH ||
      canvas::ink_playground::windows_input::arcToolForPointerType(PT_PEN) !=
          ARC_INPUT_TOOL_PEN ||
      canvas::ink_playground::windows_input::arcToolForPointerType(PT_MOUSE) !=
          ARC_INPUT_TOOL_MOUSE) {
    return 8;
  }
  const std::string trace = serializePointerTrace(
      {PointerEvidenceSample{0x1234U, 7U, 3U, 11U, "touch", "down", 10.5F,
                             20.25F, 0.5F, 2U, 6.0F, 4.0F}},
      0x1234U);
  if (trace !=
                 "{\n  \"schema_version\": \"0.1\",\n  \"device_id\": 4660,\n"
                 "  \"events\": [\n    {\"source_device_id\": 4660, \"pointer_id\": 7, "
                 "\"generation\": 3, \"timestamp_ms\": 11, \"input_type\": \"touch\", "
                 "\"phase\": \"down\", \"x\": 10.5, \"y\": 20.25, \"pressure\": 0.5, "
                 "\"batch_size\": 2, \"contact_width\": 6, \"contact_height\": 4, "
                 "\"contact_area\": 24}\n  ]\n}\n") {
    return 2;
  }

  using canvas::ink_playground::windows_input::PlatformPointerAction;
  using canvas::ink_playground::windows_input::platformPointerAction;
  using canvas::interaction::ContactDisposition;
  if (platformPointerAction(ContactDisposition::kViewportGesture, false) !=
          PlatformPointerAction::kSuppressPreview ||
      platformPointerAction(ContactDisposition::kViewportGesture, true) !=
          PlatformPointerAction::kReleaseWithoutCommit ||
      platformPointerAction(ContactDisposition::kIgnored, true) !=
          PlatformPointerAction::kReleaseWithoutCommit ||
      platformPointerAction(ContactDisposition::kPending, true) !=
          PlatformPointerAction::kCommitAndRelease ||
      platformPointerAction(ContactDisposition::kInk, true) !=
          PlatformPointerAction::kCommitAndRelease) {
    return 9;
  }

  const auto transformed = canvas::ink_playground::windows_input::toViewportPoint(
      10.0F, 20.0F, 2.0F, 3.0F, 4.0F);
  if (transformed.x != 23.0F || transformed.y != 44.0F) return 10;
  if (!canvas::ink_playground::windows_input::cancelsAllActivePointers(WM_KILLFOCUS) ||
      !canvas::ink_playground::windows_input::cancelsAllActivePointers(WM_CANCELMODE) ||
      !canvas::ink_playground::windows_input::cancelsAllActivePointers(WM_DESTROY) ||
      canvas::ink_playground::windows_input::cancelsAllActivePointers(WM_PAINT)) {
    return 11;
  }
  using canvas::interaction::MultiContactPolicy;
  if (std::wstring_view(canvas::ink_playground::windows_input::multiContactPolicyName(
          MultiContactPolicy::kAutoIntent)) != L"AutoIntent" ||
      canvas::ink_playground::windows_input::nextMultiContactPolicy(
          MultiContactPolicy::kAutoIntent) != MultiContactPolicy::kMultiInk ||
      canvas::ink_playground::windows_input::nextMultiContactPolicy(
          MultiContactPolicy::kMultiInk) != MultiContactPolicy::kGesturePriority ||
      canvas::ink_playground::windows_input::nextMultiContactPolicy(
          MultiContactPolicy::kGesturePriority) != MultiContactPolicy::kAutoIntent) {
    return 12;
  }
  return 0;
}

#else
int main() { return 0; }
#endif
