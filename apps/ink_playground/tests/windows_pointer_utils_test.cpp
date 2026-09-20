#include "../platform/windows/windows_pointer_utils.hpp"
#include "../platform/windows/windows_smoke_evidence.hpp"
#include "canvas/ink/ink_engine.hpp"

#if defined(_WIN32)

#include <cstdint>
#include <string>
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
  using canvas::ink_playground::windows_input::PointerEvidenceSample;
  using canvas::ink_playground::windows_input::serializePointerTrace;
  const std::string trace = serializePointerTrace(
      {PointerEvidenceSample{7U, 11U, "mouse", "down", 10.5F, 20.25F, 0.5F, 2U}},
      0x1234U);
  return trace ==
                 "{\n  \"schema_version\": \"0.1\",\n  \"device_id\": 4660,\n"
                 "  \"events\": [\n    {\"pointer_id\": 7, \"timestamp_ms\": 11, "
                 "\"input_type\": \"mouse\", \"phase\": \"down\", \"x\": 10.5, "
                 "\"y\": 20.25, \"pressure\": 0.5, \"batch_size\": 2}\n  ]\n}\n"
             ? 0
             : 2;
}

#else
int main() { return 0; }
#endif
