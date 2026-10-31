#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/render/skia_renderer.hpp"
#include <iostream>
#include <stdexcept>
using namespace canvas;
std::uint64_t run(unsigned n,const char* profile){
    auto p=ink::BrushPackageCatalog{}.loadDefault(profile,1).package;
    ink::BrushSession session(1,ink::resolveBrushState(p,42));if(!session.begin())throw std::runtime_error("begin");
    render::TransparentOverlaySkiaSurfaceProvider provider;if(provider.resize(128,48).code!=render::BackendSubmissionCode::kAccepted)throw std::runtime_error("resize");
    render::SkiaRenderer renderer;render::PreviewSurfaceController preview(renderer,provider);
    if(!preview.begin(1,1,1,provider.generation()))throw std::runtime_error("preview begin");
    for(unsigned i=0;i<n;++i){ink::BrushSample s{double(i)*8,32,0,false,i+1,std::uint64_t(i)*8'000'000,false};
        ink::BrushPreviewDelta delta;if(session.append({&s,1},{},delta)!=ink::BrushSessionError::kNone||!preview.update(delta)||!preview.renderIfDirty())throw std::runtime_error("append");}
    const auto work=renderer.compactWork();const auto engine=session.compactMetrics();
    std::cout<<"profile="<<profile<<" N="<<n<<" geometry_knots="<<work.generatedKnots<<" input_visits="<<engine.samples<<" simplification_visits="<<engine.simplificationVisits<<" copied_knots="<<engine.copiedKnots<<" retained_bytes="<<work.retainedBytes<<" tile_bytes="<<work.temporaryBytes<<'\n';
    if(work.retainedBytes>32*1024*1024||work.temporaryBytes>16*1024*1024)throw std::runtime_error("unbounded derived storage");
    ink::BrushCommitIntent intent;if(session.seal(intent)!=ink::BrushSessionError::kNone||session.metrics().sealEvaluations!=1)throw std::runtime_error("seal bound");
    return work.generatedKnots+engine.samples+engine.simplificationVisits+engine.copiedKnots;
}
int main(){for(const char* profile:{"round-handdrawn","marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1"}){auto a=run(512,profile),b=run(2048,profile),c=run(8192,profile);if(b>5*a||c>5*b)throw std::runtime_error("quadratic preview geometry work");}}
