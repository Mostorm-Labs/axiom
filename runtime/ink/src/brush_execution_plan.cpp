#include "canvas/ink/brush_execution_plan.hpp"
namespace canvas::ink { BrushExecutionPlan compileBrushExecutionPlan(const ResolvedBrushState& s){BrushExecutionPlan p; if(s.package.inputMode==BrushStageMode::kOff||s.package.vectorMode==BrushStageMode::kOff||s.package.renderingMode==BrushStageMode::kOff)p.error="required_stage_off"; return p;} }
