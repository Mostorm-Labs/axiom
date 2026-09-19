#include "canvas/interaction/transform_session.hpp"
#include <algorithm>
namespace canvas::interaction {
bool TransientSceneOverride::set(foundation::ObjectId id, semantic::Transform2D transform){ if(id.isZero()) return false; values_[id]=transform; return true; }
void TransientSceneOverride::clear() noexcept { values_.clear(); }
bool TransformSession::begin(std::span<const std::pair<foundation::ObjectId, semantic::Transform2D>> targets){ if(active_ || targets.empty()) return false; pending_.items.clear(); pending_.items.reserve(targets.size()); for(auto& t:targets){ if(t.first.isZero()) return false; pending_.items.push_back({t.first,t.second}); } active_=true; cancelled_=false; override_.clear(); return true; }
bool TransformSession::preview(std::span<const std::pair<foundation::ObjectId, semantic::Transform2D>> values){ if(!active_ || cancelled_ || values.size()!=pending_.items.size()) return false; for(auto& item:pending_.items){ auto it=std::find_if(values.begin(),values.end(),[&](auto& v){return v.first==item.object_id;}); if(it==values.end()) return false; item.transform=it->second; if(!override_.set(it->first,it->second)) return false; } return true; }
bool TransformSession::commit(){ if(!active_||cancelled_) return false; const auto result=submit_.submit(pending_); if(result.accepted){active_=false; override_.clear(); return true;} return false; }
void TransformSession::cancel() noexcept { active_=false; cancelled_=true; pending_.items.clear(); override_.clear(); }
TransformConflict TransformSession::onChangeSet(const semantic::ChangeSet& changes) noexcept { if(!active_) return TransformConflict::kNone; for(const auto& c:changes.objects()) if(static_cast<std::uint8_t>(c.flags)&static_cast<std::uint8_t>(semantic::SemanticChangeFlags::kDeleted)) for(const auto& i:pending_.items) if(i.object_id==c.object_id){cancel(); return TransformConflict::kCancel;} return TransformConflict::kNone; }
}
