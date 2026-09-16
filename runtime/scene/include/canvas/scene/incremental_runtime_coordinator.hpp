#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene_binding.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/semantic_read_view.hpp"

#include <cstdint>
#include <vector>

namespace canvas {

enum class RuntimeUpdateDisposition : std::uint8_t {
    kIncremental,
    kRecovery,
};

enum class RuntimeCheckpoint : std::uint8_t {
    kBeforeRuntimePrepare,
    kAfterRuntimePrepare,
    kBeforeBoundsPrepare,
    kAfterBoundsPrepare,
    kBeforeSpatialPrepare,
    kAfterSpatialPrepare,
    kBeforeInvalidationFinalization,
    kAfterInvalidationFinalization,
    kBeforePublication,
    kAfterPublication,
};

struct RuntimeUpdatePlan final {
    semantic::SemanticGeneration beforeGeneration{};
    semantic::SemanticGeneration afterGeneration{};
    RuntimeUpdateDisposition disposition = RuntimeUpdateDisposition::kIncremental;
    bool requiresFullRebuild = false;
    std::vector<semantic::ObjectId> affectedObjects;
};

class IncrementalRuntimeCoordinator final {
  public:
    explicit IncrementalRuntimeCoordinator(SceneBinding& binding) noexcept : binding_(binding) {
        runtimeScene_.setPublicationGate(&publicationGate_);
        binding_.setPublicationGate(&publicationGate_);
    }

    [[nodiscard]] foundation::Result<RuntimeUpdatePlan> plan(
        const semantic::SemanticReadView& postState,
        const semantic::ChangeSet& changes) const;

    foundation::Result<SceneSyncReceipt> apply(
        const ISemanticSceneCompiler& compiler,
        const SceneCommitInput& input);

    foundation::Result<SceneSyncReceipt> recover(
        const ISemanticSceneCompiler& compiler,
        const SceneCommitInput& input);

    [[nodiscard]] const RuntimeScene& runtimeScene() const noexcept {
        return runtimeScene_;
    }
    [[nodiscard]] bool publicationObservationCoherent() const noexcept {
        return publicationObservationCoherent_;
    }

  private:
    friend class IncrementalRuntimeTestAccess;
    using TestPublicationObserver = void (*)(void*) noexcept;
    void setTestPublicationObserver(TestPublicationObserver observer, void* context) noexcept {
        testPublicationObserver_ = observer;
        testPublicationObserverContext_ = context;
    }

    void setCheckpointFailure(RuntimeCheckpoint checkpoint) noexcept {
        checkpointFailure_ = checkpoint;
    }
    void clearCheckpointFailure() noexcept { checkpointFailure_.reset(); }
    void corruptRuntimeProjectionForTest() noexcept {
        runtimeScene_.corruptForTest();
    }
    [[nodiscard]] bool checkpointFails(RuntimeCheckpoint checkpoint) const noexcept {
        return checkpointFailure_.has_value() && *checkpointFailure_ == checkpoint;
    }
    static bool transactionCheckpoint(void* context, std::uint8_t checkpoint) noexcept;
    static void publishPending(void* context) noexcept;
    static void observePublication(void* context) noexcept;
    void abortPublication() noexcept;

    SceneBinding& binding_;
    RuntimeScene runtimeScene_;
    std::optional<RuntimeCheckpoint> checkpointFailure_;
    std::optional<RuntimeScene::PreparedPublication> pendingPublication_;
    ScenePublicationGate publicationGate_;
    semantic::SemanticGeneration pendingGeneration_{};
    SceneRevision pendingRevision_{};
    std::uint64_t publicationObservations_ = 0;
    bool publicationObservationCoherent_ = true;
    TestPublicationObserver testPublicationObserver_ = nullptr;
    void* testPublicationObserverContext_ = nullptr;
};

} // namespace canvas
