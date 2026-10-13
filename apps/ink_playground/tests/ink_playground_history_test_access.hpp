#pragma once

#include "ink_playground_host.hpp"
#include "../../../runtime/scene/tests/incremental_runtime_test_access.hpp"
#include <chrono>

namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess final {
 public:
  static std::vector<semantic::ObjectRecord> objects(const InkPlaygroundHost& host) {
    return host.semanticObjects_.allObjects();
  }
  static std::vector<RuntimeSceneRecord> runtimeRecords(const InkPlaygroundHost& host) {
    const auto records = host.sceneCoordinator_->runtimeScene().records();
    return {records.begin(), records.end()};
  }
  static QualificationObservation observeQualification(const InkPlaygroundHost& host) {
    QualificationObservation result;
    const auto topLeft = host.viewToContent(0, 0);
    const auto bottomRight = host.viewToContent(
        static_cast<float>(host.surface().width), static_cast<float>(host.surface().height));
    const auto started = std::chrono::steady_clock::now();
    const auto queried = host.runtimeSceneHost_->query(SceneQuery{
        WorldRect{topLeft.first, topLeft.second, bottomRight.first, bottomRight.second}});
    result.queryMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    if (queried) result.candidatesExamined = queried.value().diagnostics.candidatesExamined;
    const auto& invalidation = host.runtimeSceneHost_->invalidationOutput();
    result.invalidationRectCount = invalidation.rects.size();
    result.fullSceneInvalidation = invalidation.fullScene;
    result.sceneGeneration = host.sceneRevision();
    result.cameraGeneration = host.cameraGeneration();
    result.canonicalOperationCount = host.submittedOperationCount();
    result.overlayUpdates = host.selectionOverlayUpdates();
    return result;
  }
  static std::vector<SceneRecord> sceneRecords(const InkPlaygroundHost& host) {
    const auto records = host.runtimeSceneHost_->read().records();
    return {records.begin(), records.end()};
  }
  static auto query(const InkPlaygroundHost& host, WorldRect bounds) {
    return host.runtimeSceneHost_->query(SceneQuery{bounds});
  }
  static auto invalidation(const InkPlaygroundHost& host) {
    return host.runtimeSceneHost_->invalidationOutput();
  }
  static void failPublication(InkPlaygroundHost& host) noexcept {
    IncrementalRuntimeTestAccess::failAt(*host.sceneCoordinator_, RuntimeCheckpoint::kBeforePublication);
  }
  static void clearFailure(InkPlaygroundHost& host) noexcept {
    IncrementalRuntimeTestAccess::clear(*host.sceneCoordinator_);
  }
  static void exhaustOperationIds(InkPlaygroundHost& host) noexcept {
    host.nextHistoryOperationOrdinal_ = 0U;
  }
  static bool applyRemote(InkPlaygroundHost& host, semantic::OperationPayload payload) {
    const semantic::Operation op{host.allocateOperationId(),host.documentId_,1,1,std::move(payload)};
    return host.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition ==
        semantic::ApplyDisposition::kApplied;
  }
};
}  // namespace canvas::ink_playground
