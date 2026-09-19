#include "canvas/ink/ink_engine.hpp"
#include "canvas/ink/brush_engine.hpp"
namespace canvas::ink {
bool InkEngine::begin(std::uint64_t id) noexcept { if(active_ || id==0) return false; id_=id; points_.clear(); processed_=0; cancelled_=false; active_=true; return true; }
bool InkEngine::append(const input::PointerSample& sample) noexcept { if(!active_ || sample.predicted || sample.sequence==0 || sample.timestampNs==0) return false; if(!points_.empty() && sample.sequence<=points_.size()) return false; points_.push_back(BrushEngine::point(brush_,sample)); ++processed_; return true; }
std::optional<StrokeRecord> InkEngine::finish() { if(!active_ || points_.empty()) return std::nullopt; active_=false; return StrokeRecord{id_,points_}; }
}
