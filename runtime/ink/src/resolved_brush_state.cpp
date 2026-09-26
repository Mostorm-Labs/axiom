#include "canvas/ink/resolved_brush_state.hpp"
#include <functional>
namespace canvas::ink { ResolvedBrushState resolveBrushState(const BrushPackage& p,std::uint64_t seed){ResolvedBrushState r; r.package=p; r.seed=seed; r.identity=std::to_string(std::hash<std::string>{}(p.packageId+"/"+std::to_string(p.revision)+"/"+std::to_string(seed))); return r;} }
