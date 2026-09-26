#include "canvas/ink/brush_session.hpp"
#include <cassert>
#include <array>
int main(){
  canvas::ink::BrushPackage p; p.packageId="0123456789abcdef0123456789abcdef";
  auto state=canvas::ink::resolveBrushState(p,42);
  canvas::ink::BrushSession s(7,state); assert(s.begin());
  const std::array c1{canvas::ink::BrushSample{0,0,0.1,true,1},canvas::ink::BrushSample{8,0,0.9,true,2}};
  const std::array pred{canvas::ink::BrushSample{16,0,0.8,true,100}};
  canvas::ink::BrushPreviewDelta d; assert(s.append(c1,pred,d)==canvas::ink::BrushSessionError::kNone); assert(!d.outline.empty());
  const std::array c2{canvas::ink::BrushSample{16,0,0.2,true,3}}; assert(s.append(c2,{},d)==canvas::ink::BrushSessionError::kNone);
  canvas::ink::BrushCommitIntent intent; assert(s.seal(intent)==canvas::ink::BrushSessionError::kNone);
  assert(intent.seed==42 && intent.confirmed.size()==3 && !intent.outline.empty());
  for (const auto& sample : intent.confirmed) assert(!sample.pressurePresent);
  canvas::ink::BrushSession sameGeometry(8,state); assert(sameGeometry.begin());
  const std::array different{canvas::ink::BrushSample{0,0,0.9,true,1},canvas::ink::BrushSample{8,0,0.1,true,2},canvas::ink::BrushSample{16,0,0.7,true,3}};
  canvas::ink::BrushPreviewDelta other; assert(sameGeometry.append(different,{},other)==canvas::ink::BrushSessionError::kNone);
  canvas::ink::BrushSession samePressure(9,state); assert(samePressure.begin());
  canvas::ink::BrushPreviewDelta expectedPreview; assert(samePressure.append(c1,{},expectedPreview)==canvas::ink::BrushSessionError::kNone);
  canvas::ink::BrushPreviewDelta expectedPreview2; assert(samePressure.append(c2,{},expectedPreview2)==canvas::ink::BrushSessionError::kNone);
  assert(other.outline.size() == expectedPreview2.outline.size());
  for (std::size_t i = 0; i < other.outline.size(); ++i) {
    assert(other.outline[i].x == expectedPreview2.outline[i].x);
    assert(other.outline[i].y == expectedPreview2.outline[i].y);
  }

  // The published preview is the complete retained stroke geometry.  It is
  // replaced on every revision, so a long in-progress stroke cannot collapse
  // to only the bounded incremental tail while the pointer is still down.
  canvas::ink::BrushSession longStroke(10, state);
  assert(longStroke.begin());
  std::array<canvas::ink::BrushSample, 20> longSamples{};
  for (std::size_t i = 0; i < longSamples.size(); ++i) {
    longSamples[i] = {static_cast<double>(i * 8U), 0.0, 0.5, true,
                      static_cast<std::uint64_t>(i + 1U)};
  }
  canvas::ink::BrushPreviewDelta longPreview;
  assert(longStroke.append(longSamples, {}, longPreview) == canvas::ink::BrushSessionError::kNone);
  canvas::ink::BrushCommitIntent longIntent;
  assert(longStroke.seal(longIntent) == canvas::ink::BrushSessionError::kNone);
  assert(longPreview.outline.size() > 16U);
  double previewMinX = longPreview.outline.front().x;
  double previewMaxX = longPreview.outline.front().x;
  for (const auto& point : longPreview.outline) {
    previewMinX = std::min(previewMinX, point.x);
    previewMaxX = std::max(previewMaxX, point.x);
  }
  assert(previewMinX <= 0.0 && previewMaxX >= 152.0);
  assert(!longIntent.outline.empty());
  return 0;
}
