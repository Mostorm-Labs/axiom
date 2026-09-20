#include "canvas/ink/ink_engine.hpp"
#include "canvas/ink/brush_engine.hpp"
namespace canvas::ink {
bool InkEngine::begin(std::uint64_t id) noexcept { if(active_ || id==0) return false; id_=id; points_.clear(); processed_=0; cancelled_=false; active_=true; return true; }
bool InkEngine::begin(const input::PointerKey& key, std::uint64_t id) noexcept {
  if (!key.valid() || id == 0 || sessions_.contains(key)) return false;
  sessions_.emplace(key, Session{id, {}, 0});
  return true;
}
bool InkEngine::append(const input::PointerSample& sample) noexcept {
  if (sample.key.valid()) return append(sample.key, sample);
  if(!active_ || sample.predicted || sample.sequence==0 || sample.timestampNs==0) return false; if(!points_.empty() && sample.sequence<=points_.size()) return false; points_.push_back(BrushEngine::point(brush_,sample)); ++processed_; return true;
}
bool InkEngine::append(const input::PointerKey& key, const input::PointerSample& sample) noexcept {
  auto it = sessions_.find(key);
  if (it == sessions_.end() || sample.predicted || sample.sequence == 0 || sample.timestampNs == 0) return false;
  if (!it->second.points.empty() && sample.sequence <= it->second.points.size()) return false;
  it->second.points.push_back(BrushEngine::point(brush_, sample));
  ++it->second.processed;
  return true;
}
std::optional<StrokeRecord> InkEngine::finish() { if(!active_ || points_.empty()) return std::nullopt; active_=false; return StrokeRecord{id_,points_}; }
std::optional<StrokeRecord> InkEngine::finish(const input::PointerKey& key) {
  auto it = sessions_.find(key);
  if (it == sessions_.end() || it->second.points.empty()) return std::nullopt;
  StrokeRecord result{it->second.id, std::move(it->second.points)};
  sessions_.erase(it);
  return result;
}
void InkEngine::cancel(const input::PointerKey& key) noexcept { sessions_.erase(key); }
}
