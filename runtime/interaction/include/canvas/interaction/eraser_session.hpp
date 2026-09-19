#pragma once
#include "canvas/interaction/interaction_ports.hpp"
#include "canvas/semantic/operation_payload.hpp"
#include <span>
namespace canvas::interaction {
struct EraseCandidate final { foundation::ObjectId objectId{}; semantic::ObjectKind kind=semantic::ObjectKind::kShape; bool visible=true; bool locked=false; };
class EraseSubmitPort { public: virtual ~EraseSubmitPort()=default; virtual SubmitResult submit(const semantic::DeleteObjectsOp&)=0; };
class EraserSession final {
 public: explicit EraserSession(EraseSubmitPort& submit) noexcept:submit_(submit){} bool begin() noexcept; bool sweep(std::span<const EraseCandidate> candidates); bool commit(); void cancel() noexcept;
 private: EraseSubmitPort& submit_; semantic::DeleteObjectsOp pending_; bool active_=false;
};
}
