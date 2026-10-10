#include "canvas/debug_ui/snapshot_assembler.hpp"

#include <chrono>

namespace canvas::debug_ui {
namespace {
using Identity = canvas::runtime::RuntimeIdentityState;

bool sameIdentity(const Identity& left, const Identity& right) noexcept {
  return left.runtimeGeneration == right.runtimeGeneration &&
         left.documentGeneration == right.documentGeneration &&
         left.documentRevision == right.documentRevision &&
         left.viewGeneration == right.viewGeneration &&
         left.surfaceGeneration == right.surfaceGeneration;
}

void readNonProduct(const DebugSnapshotSources& sources, DebugSnapshot& snapshot) noexcept {
  if (sources.axiom != nullptr) {
    snapshot.axiom.availability = DebugAvailability::kAvailable;
    snapshot.axiom.value = sources.axiom->readDiagnostics();
  }
  if (sources.arc != nullptr) {
    snapshot.arc.availability = DebugAvailability::kAvailable;
    snapshot.arc.value = sources.arc->readArcDiagnostics();
  }
  if (sources.platform != nullptr) {
    snapshot.platform.value = sources.platform->readPlatformDiagnostics();
    snapshot.platform.availability = snapshot.platform.value.surfaceAvailable
        ? DebugAvailability::kAvailable : DebugAvailability::kDegraded;
  }
  if (sources.telemetry != nullptr) {
    snapshot.telemetry.availability = DebugAvailability::kAvailable;
    snapshot.telemetry.value = sources.telemetry->readTelemetry();
  }
  if (sources.activity != nullptr) snapshot.activity = sources.activity->readActivity();
}

void finalizeStamp(DebugSnapshot& snapshot) noexcept {
  snapshot.stamp.snapshotSequence = snapshot.stamp.snapshotSequence;
  if (snapshot.telemetry.availability == DebugAvailability::kAvailable) {
    snapshot.stamp.sequence = snapshot.telemetry.value.sequence;
    snapshot.stamp.frameId = snapshot.telemetry.value.canonicalFrames;
  } else if (snapshot.platform.availability != DebugAvailability::kUnsupported) {
    snapshot.stamp.sequence = snapshot.stamp.snapshotSequence;
    snapshot.stamp.frameId = snapshot.platform.value.presentCount;
  } else {
    snapshot.stamp.sequence = snapshot.stamp.snapshotSequence;
  }
  snapshot.stamp.monotonicTimeNs = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count());
  if (snapshot.product.availability == DebugAvailability::kAvailable) {
    const auto& identity = snapshot.product.value.identity;
    snapshot.stamp.runtimeGeneration = identity.runtimeGeneration;
    snapshot.stamp.documentGeneration = identity.documentGeneration;
    snapshot.stamp.documentRevision = identity.documentRevision;
    snapshot.stamp.viewGeneration = identity.viewGeneration;
    snapshot.stamp.surfaceGeneration = identity.surfaceGeneration;
    snapshot.stamp.generation = identity.surfaceGeneration;
  }
}

}  // namespace

DebugSnapshot DebugSnapshotAssembler::capture(const DebugSnapshotSources& sources) const noexcept {
  DebugSnapshot snapshot{};
  snapshot.stamp.snapshotSequence = ++snapshotSequence_;
  if (sources.runtime == nullptr) {
    readNonProduct(sources, snapshot);
    snapshot.coherence = SnapshotCoherence::kStale;
    finalizeStamp(snapshot);
    return snapshot;
  }

  for (int attempt = 0; attempt < 2; ++attempt) {
    snapshot = {};
    snapshot.stamp.snapshotSequence = snapshotSequence_;
    const auto leading = sources.runtime->readRuntimeState();
    readNonProduct(sources, snapshot);
    const auto trailing = sources.runtime->readRuntimeState();
    snapshot.product.availability = DebugAvailability::kAvailable;
    snapshot.product.value = trailing;
    if (sameIdentity(leading.identity, trailing.identity)) {
      snapshot.coherence = SnapshotCoherence::kCoherent;
      finalizeStamp(snapshot);
      return snapshot;
    }
    if (attempt == 1) {
      snapshot.coherence = SnapshotCoherence::kMixedGeneration;
      finalizeStamp(snapshot);
      return snapshot;
    }
  }
  snapshot.coherence = SnapshotCoherence::kMixedGeneration;
  finalizeStamp(snapshot);
  return snapshot;
}

}  // namespace canvas::debug_ui
