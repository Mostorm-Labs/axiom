#include "canvas/interaction/eraser_session.hpp"
#include <algorithm>
namespace canvas::interaction {
bool EraserSession::begin() noexcept { if(active_)return false; pending_.object_ids.clear(); active_=true; return true; }
bool EraserSession::sweep(std::span<const EraseCandidate> candidates){ if(!active_)return false; for(const auto& c:candidates){ const bool ink=c.kind==semantic::ObjectKind::kVectorStroke||c.kind==semantic::ObjectKind::kDabStroke; if(ink&&c.visible&&!c.locked&&!c.objectId.isZero()&&std::find(pending_.object_ids.begin(),pending_.object_ids.end(),c.objectId)==pending_.object_ids.end())pending_.object_ids.push_back(c.objectId);} return true; }
bool EraserSession::commit(){ if(!active_||pending_.object_ids.empty())return false; const auto r=submit_.submit(pending_); if(r.accepted){active_=false;pending_.object_ids.clear();return true;}return false; }
void EraserSession::cancel() noexcept {active_=false;pending_.object_ids.clear();}
}
