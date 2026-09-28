#include "canvas/ink/brush_session.hpp"
#include <cassert>
#include <array>
#include <cmath>
#include <cstdio>
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

  canvas::ink::BrushPackage chalk;
  chalk.packageId = "0123456789abcdef0123456789abcdef";
  chalk.profileId = "chalk-grain-v1";
  chalk.revision = 2U;
  chalk.material = canvas::ink::BrushMaterialMode::kChalkGrain;
  chalk.grain.spacing = 2.0;
  chalk.grain.density = 0.65;
  chalk.grain.opacity = 0.55;
  auto chalkState = canvas::ink::resolveBrushState(chalk, 99U);
  canvas::ink::BrushSession chalkSession(11U, chalkState);
  assert(chalkSession.begin());
  std::array<canvas::ink::BrushSample, 4> chalkSamples{{
      {0.0, 0.0, 0.5, true, 1}, {8.0, 0.0, 0.5, true, 2},
      {16.0, 0.0, 0.5, true, 3}, {24.0, 0.5, 0.5, true, 4}}};
  canvas::ink::BrushPreviewDelta chalkPreview;
  assert(chalkSession.append(chalkSamples, {}, chalkPreview) == canvas::ink::BrushSessionError::kNone);
  assert(!chalkPreview.dabs.empty());
  // A preview append may evaluate the normalized path once, not run a
  // second full-history finalization just to place dabs.
  assert(chalkSession.metrics().appendEvaluations == 1U);
  assert(chalkSession.metrics().sealEvaluations == 0U);
  canvas::ink::BrushCommitIntent chalkIntent;
  assert(chalkSession.seal(chalkIntent) == canvas::ink::BrushSessionError::kNone);
  assert(chalkIntent.dabs == chalkPreview.dabs);
  assert(chalkIntent.dabDigest != 0U);

  canvas::ink::BrushSession resampled(12U, chalkState);
  assert(resampled.begin());
  const std::array<canvas::ink::BrushSample, 7> resampledInput{{
      {0.0, 0.0, 0.5, true, 1}, {4.0, 0.0, 0.5, true, 2},
      {8.0, 0.0, 0.5, true, 3}, {12.0, 0.0, 0.5, true, 4},
      {16.0, 0.0, 0.5, true, 5}, {20.0, 0.0, 0.5, true, 6},
      {24.0, 0.5, 0.5, true, 7}}};
  canvas::ink::BrushPreviewDelta resampledPreview;
  assert(resampled.append(resampledInput, {}, resampledPreview) == canvas::ink::BrushSessionError::kNone);
  assert(resampledPreview.dabs.size() == chalkPreview.dabs.size());
  return 0;
}
