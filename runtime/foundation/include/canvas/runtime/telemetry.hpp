#pragma once

#include <cstdint>

namespace canvas::runtime {

struct TelemetrySnapshot final {
  std::uint64_t sequence = 0;
  std::uint64_t inputEvents = 0;
  std::uint64_t previewFrames = 0;
  std::uint64_t canonicalFrames = 0;
  double inputToPreviewMs = 0.0;
};

class TelemetryProvider {
 public:
  virtual ~TelemetryProvider() = default;
};

class Telemetry : public TelemetryProvider {
 public:
  ~Telemetry() override = default;
  [[nodiscard]] virtual TelemetrySnapshot readTelemetry() const noexcept = 0;
};

using ITelemetry = Telemetry;

}  // namespace canvas::runtime
