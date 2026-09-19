#pragma once
#include "canvas/interaction/interaction_ports.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/operation_payload.hpp"
#include <span>
#include <unordered_map>
#include <vector>
namespace canvas::interaction {
class TransformSubmitPort { public: virtual ~TransformSubmitPort()=default; virtual SubmitResult submit(const semantic::SetTransformsOp&)=0; };
class TransientSceneOverride final {
 public:
  bool set(foundation::ObjectId id, semantic::Transform2D transform);
  void clear() noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
 private: friend class TransformSession; std::unordered_map<foundation::ObjectId, semantic::Transform2D, foundation::ObjectIdHash> values_;
};
enum class TransformConflict { kNone, kCancel, kReResolve };
class TransformSession final {
 public:
  TransformSession(TransformSubmitPort& submit, TransientSceneOverride& overrideState) noexcept : submit_(submit), override_(overrideState) {}
  bool begin(std::span<const std::pair<foundation::ObjectId, semantic::Transform2D>> targets);
  bool preview(std::span<const std::pair<foundation::ObjectId, semantic::Transform2D>> values);
  bool commit(); void cancel() noexcept;
  TransformConflict onChangeSet(const semantic::ChangeSet& changes) noexcept;
 private: TransformSubmitPort& submit_; TransientSceneOverride& override_; semantic::SetTransformsOp pending_; bool active_=false; bool cancelled_=false;
};
}
