#pragma once
#include "canvas/input/pointer_sample.hpp"
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>
namespace canvas::ink {
struct BrushDescriptor final { float diameter=1.0F; };
struct StrokePoint final { float x=0; float y=0; float pressure=0; bool operator==(const StrokePoint&) const = default; };
struct StrokeRecord final { std::uint64_t id=0; std::vector<StrokePoint> points; };
class InkEngine final {
 public:
  explicit InkEngine(BrushDescriptor brush) noexcept:brush_(brush){}
  bool begin(std::uint64_t id) noexcept;
  bool begin(const input::PointerKey& key, std::uint64_t id) noexcept;
  bool append(const input::PointerSample& sample) noexcept;
  bool append(const input::PointerKey& key, const input::PointerSample& sample) noexcept;
  std::size_t previewPointCount() const noexcept { return points_.size(); }
  std::optional<StrokeRecord> finish();
  std::optional<StrokeRecord> finish(const input::PointerKey& key);
  void cancel() noexcept { points_.clear(); cancelled_=true; active_=false; }
  void cancel(const input::PointerKey& key) noexcept;
  bool cancelled() const noexcept { return cancelled_; }
  std::size_t processedSampleCount() const noexcept { return processed_; }
  [[nodiscard]] std::size_t activeStrokeCount() const noexcept { return sessions_.size(); }
 private: BrushDescriptor brush_; std::uint64_t id_=0; std::vector<StrokePoint> points_; std::size_t processed_=0; bool active_=false; bool cancelled_=false;
  struct Session final { std::uint64_t id=0; std::vector<StrokePoint> points; std::size_t processed=0; };
  std::unordered_map<input::PointerKey, Session, input::PointerKeyHash> sessions_;
};
}
