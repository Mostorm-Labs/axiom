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
