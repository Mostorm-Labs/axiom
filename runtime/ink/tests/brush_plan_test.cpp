#include "canvas/ink/brush_execution_plan.hpp"
#include <array>
#include <cassert>
int main(){
  canvas::ink::BrushPackage p; p.packageId="0123456789abcdef0123456789abcdef";
  auto state=canvas::ink::resolveBrushState(p,0); auto plan=canvas::ink::compileBrushExecutionPlan(state);
  assert(plan); const auto expected=std::array<std::uint32_t,3>{1,2,3}; assert(plan.active==expected);
  p.vectorMode=canvas::ink::BrushStageMode::kOff;
  const auto rejected=canvas::ink::compileBrushExecutionPlan(canvas::ink::resolveBrushState(p,0)); assert(!rejected);
  return 0;
}
