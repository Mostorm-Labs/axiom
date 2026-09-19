#pragma once
#include "canvas/input/pointer_sample.hpp"
#include <cstdint>
#include <optional>
#include <vector>
namespace canvas::ink {
struct BrushDescriptor final { float diameter=1.0F; };
struct StrokePoint final { float x=0; float y=0; float pressure=0; bool operator==(const StrokePoint&) const = default; };
struct StrokeRecord final { std::uint64_t id=0; std::vector<StrokePoint> points; };
class InkEngine final {
 public:
  explicit InkEngine(BrushDescriptor brush) noexcept:brush_(brush){}
  bool begin(std::uint64_t id) noexcept;
  bool append(const input::PointerSample& sample) noexcept;
  std::size_t previewPointCount() const noexcept { return points_.size(); }
  std::optional<StrokeRecord> finish();
  void cancel() noexcept { points_.clear(); cancelled_=true; active_=false; }
  bool cancelled() const noexcept { return cancelled_; }
  std::size_t processedSampleCount() const noexcept { return processed_; }
 private: BrushDescriptor brush_; std::uint64_t id_=0; std::vector<StrokePoint> points_; std::size_t processed_=0; bool active_=false; bool cancelled_=false;
};
}
