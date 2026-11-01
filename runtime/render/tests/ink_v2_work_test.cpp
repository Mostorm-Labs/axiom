#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/render/skia_renderer.hpp"
#include <iostream>
#include <stdexcept>
using namespace canvas;
std::uint64_t run(unsigned n,const char* profile,unsigned revision=1){
    auto p=ink::BrushPackageCatalog{}.loadDefault(profile,revision).package;
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
void retinaPrefix(){
    using namespace canvas;
    for(const auto profile:{"crayon-wax-v1","chalk-compact-v1","watercolor-lite-v1"}){
    auto p=ink::BrushPackageCatalog{}.loadDefault(profile,3).package;p.compactConfig->size=32;
    ink::BrushSession session(1,ink::resolveBrushState(p,42));if(!session.begin())throw std::runtime_error("retina begin");
    render::TransparentOverlaySkiaSurfaceProvider surface;if(surface.resize(2000,1300).code!=render::BackendSubmissionCode::kAccepted)throw std::runtime_error("retina surface");
    render::SkiaRenderer renderer;render::PreviewSurfaceController preview(renderer,surface);
    if(!preview.begin(1,1,1,surface.generation()))throw std::runtime_error("retina preview");
    preview.setSurfaceMetrics({1000,650,2000,1300,2,2});
    for(unsigned i=0;i<80;++i){ink::BrushSample sample{20.+i*18,300,0,false,i+1,std::uint64_t(i)*8'000'000};ink::BrushPreviewDelta delta;
        if(session.append({&sample,1},{},delta)!=ink::BrushSessionError::kNone||!preview.update(delta)||!preview.renderIfDirty())throw std::runtime_error("retina input");}
    const auto work=renderer.compactWork();
    std::cout<<"retina rendered_knots="<<work.generatedKnots<<" retained_bytes="<<work.retainedBytes<<" temporary_bytes="<<work.temporaryBytes<<std::endl;
    if(!work.retainedBytes||work.generatedKnots>80*24)throw std::runtime_error("retina full-history preview fallback");
    if(work.retainedBytes+work.temporaryBytes>32*1024*1024)throw std::runtime_error("retina cache budget");
    const auto submits=renderer.submissionCount();if(!preview.renderIfDirty()||renderer.submissionCount()!=submits)throw std::runtime_error("retina no-op resubmits");
    for(unsigned contact=2;contact<5;++contact){
        auto chunk=std::make_shared<ink::BrushPreviewDelta::Chunk>();chunk->knots={{20,100.+contact*30,32768},{800,100.+contact*30,32768}};
        ink::BrushPreviewDelta delta;delta.revision=1;delta.compactConfig=p.compactConfig;delta.compactSeed=contact;delta.compactChunks=chunk;
        if(!preview.begin(1,contact,1,surface.generation())||!preview.updateForSession(contact,delta)||!preview.renderIfDirty())throw std::runtime_error("retina contacts");
        const auto w=renderer.compactWork();if(w.retainedBytes+w.temporaryBytes>32*1024*1024)throw std::runtime_error("multi-contact retina budget");
    }
    }
}
int main(int argc,char** argv){if(argc>1){retinaPrefix();return 0;}for(const char* profile:{"round-handdrawn","marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1"}){auto a=run(512,profile),b=run(2048,profile),c=run(8192,profile);if(b>5*a||c>5*b)throw std::runtime_error("quadratic preview geometry work");}
    for(const auto profile:{"crayon-wax-v1","chalk-compact-v1","watercolor-lite-v1"}){
    auto a=run(512,profile,3),b=run(2048,profile,3),c=run(8192,profile,3);
    if(b>5*a||c>5*b)throw std::runtime_error("quadratic pressure material preview work");}}
