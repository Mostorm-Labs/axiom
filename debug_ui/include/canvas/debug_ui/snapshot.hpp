#pragma once

#include "canvas/debug_ui/activity.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/runtime/telemetry.hpp"

#include <array>
#include <cstdint>
#include <mutex>

namespace canvas::debug_ui {

enum class DebugAvailability : std::uint8_t {
  kAvailable,
  kDegraded,
  kUnavailable,
  kUnsupported,
  kError,
};

enum class SnapshotCoherence : std::uint8_t {
  kCoherent,
  kMixedGeneration,
  kStale,
};

template <typename T>
struct DiagnosticSection final {
  DebugAvailability availability = DebugAvailability::kUnsupported;
  T value{};
};

struct DebugSnapshotStamp final {
  std::uint64_t generation = 0;
  std::uint64_t sequence = 0;
  std::uint64_t snapshotSequence = 0;
  std::uint64_t monotonicTimeNs = 0;
  std::uint64_t frameId = 0;
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t documentGeneration = 0;
  std::uint64_t documentRevision = 0;
  std::uint64_t viewGeneration = 0;
  std::uint64_t surfaceGeneration = 0;
  friend bool operator==(const DebugSnapshotStamp&, const DebugSnapshotStamp&) = default;
};

struct DebugSnapshot final {
  DebugSnapshotStamp stamp{};
  SnapshotCoherence coherence = SnapshotCoherence::kStale;
  DiagnosticSection<canvas::runtime::RuntimeStateSnapshot> product{};
  DiagnosticSection<canvas::runtime::AxiomDiagnosticsSnapshot> axiom{};
  DiagnosticSection<canvas::runtime::ArcDiagnosticsSnapshot> arc{};
  DiagnosticSection<canvas::runtime::PlatformDiagnosticsSnapshot> platform{};
  DiagnosticSection<canvas::runtime::TelemetrySnapshot> telemetry{};
  DebugActivitySnapshot activity{};
};

class MutexCopySnapshotChannel final {
 public:
  MutexCopySnapshotChannel();
  void publish(DebugSnapshot snapshot);
  [[nodiscard]] DebugSnapshot read() const;
  [[nodiscard]] DebugSnapshotStamp stamp() const;

 private:
  mutable std::mutex mutex_;
  DebugSnapshot snapshot_{};
};

}  // namespace canvas::debug_ui
