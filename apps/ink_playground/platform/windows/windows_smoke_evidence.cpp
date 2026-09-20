#include "windows_smoke_evidence.hpp"

#if defined(_WIN32)

#include <iomanip>
#include <sstream>

namespace canvas::ink_playground::windows_input {

std::string serializePointerTrace(
    const std::vector<PointerEvidenceSample>& samples, std::uint64_t deviceId) {
  std::ostringstream out;
  out << "{\n  \"schema_version\": \"0.1\",\n  \"device_id\": " << deviceId
      << ",\n  \"events\": [";
  if (!samples.empty()) out << "\n";
  out << std::setprecision(9);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const auto& sample = samples[i];
    out << "    {\"pointer_id\": " << sample.pointerId
        << ", \"timestamp_ms\": " << sample.timestampMs
        << ", \"input_type\": \"" << sample.inputType
        << "\", \"phase\": \"" << sample.phase
        << "\", \"x\": " << sample.x << ", \"y\": " << sample.y
        << ", \"pressure\": " << sample.pressure
        << ", \"batch_size\": " << sample.batchSize << "}";
    if (i + 1U != samples.size()) out << ",";
    out << "\n";
  }
  out << "  ]\n}\n";
  return out.str();
}

}  // namespace canvas::ink_playground::windows_input

#endif
