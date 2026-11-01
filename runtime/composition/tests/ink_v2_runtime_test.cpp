#include "canvas/interaction/canvas_editing_runtime.hpp"
#include <stdexcept>
#include <iostream>
using namespace canvas;
class Inspect final:public interaction::CanvasEditingRuntime {
public: auto objects() const{return semanticObjects_.allObjects();}
};
void previewTint(const char* profile){
    Inspect runtime;
    if(!runtime.bindSurface(256,256)||!runtime.registerInputSource(71,"ink-v2-preview")||
       !runtime.selectBrushProfile(profile,1)||!runtime.setInkOptions(24,.1F,.3F,.8F,1,.65F))
        throw std::runtime_error("preview prepare");
    runtime.setPlatformPresentationDeferred(true);
    input::PlatformPointerBatch batch;
    for(std::uint64_t i=0;i<3;++i){
        input::PlatformPointerSample s;s.source=71;s.pointer=1;s.sequence=i+1;
        s.timestampNs=(i+1)*10'000'000;s.x=40+float(i)*40;s.y=100;
        s.phase=i==0?input::PointerPhase::kDown:input::PointerPhase::kMove;
        batch.samples.push_back(s);
    }
    if(!runtime.acceptPlatformBatch(batch,batch.samples.back().timestampNs))
        throw std::runtime_error("preview input");
    interaction::PreviewPresentationCapture capture;
    if(!runtime.capturePreviewPresentation(capture)||!runtime.renderPreviewPresentation(capture)||
       runtime.semanticObjectCount()!=0)throw std::runtime_error("live preview missing or canonical mutated");
    std::vector<std::uint8_t> pixels(256*256*4);
    if(runtime.previewSurfaceProvider()->readbackRgba(pixels).code!=render::BackendSubmissionCode::kAccepted)
        throw std::runtime_error("preview readback");
    std::uint64_t red=0,green=0,blue=0,alpha=0;
    for(std::size_t i=0;i<pixels.size();i+=4){red+=pixels[i];green+=pixels[i+1];blue+=pixels[i+2];alpha+=pixels[i+3];}
    if(!alpha||red<=green||green<=blue)throw std::runtime_error(std::string(profile)+" preview must be amber before commit");
    auto up=batch.samples.back();up.phase=input::PointerPhase::kUp;++up.sequence;up.timestampNs+=10'000'000;
    if(!runtime.acceptPlatformBatch({{up}},up.timestampNs)||runtime.objects().size()!=1)
        throw std::runtime_error("preview commit");
    const auto objects=runtime.objects();
    const auto& config=std::get<semantic::CompactStrokeContent>(objects.front().content).stroke.config;
    if(config.color!=std::array<double,4>{double(.1F),double(.3F),double(.8F),1}||config.opacity!=double(.65F))
        throw std::runtime_error("preview tint leaked into canonical brush config");
    std::cout<<profile<<" amber preview preserves canonical paint\n";
}
void run(const char* profile){
    Inspect runtime;
    if(!runtime.bindSurface(256,256)||!runtime.registerInputSource(71,"ink-v2")||!runtime.selectBrushProfile(profile,1))throw std::runtime_error("compact runtime prepare");
    runtime.setPlatformPresentationDeferred(true);
    input::PlatformPointerBatch batch;
    for(std::uint64_t i=0;i<3;++i){input::PlatformPointerSample s;s.source=71;s.pointer=1;s.sequence=i+1;s.timestampNs=(i+1)*10'000'000;s.x=40+float(i)*40;s.y=100;s.phase=i==0?input::PointerPhase::kDown:i==2?input::PointerPhase::kUp:input::PointerPhase::kMove;batch.samples.push_back(s);}
    if(!runtime.acceptPlatformBatch(batch,batch.samples.back().timestampNs))throw std::runtime_error("compact runtime input rejected");
    const auto objects=runtime.objects();
    if(objects.size()!=1||!std::holds_alternative<semantic::CompactStrokeContent>(objects[0].content))throw std::runtime_error("compact input did not commit unique carrier");
    if(!runtime.presentCanonicalFrame(runtime.nextCanonicalFrameId(),0))throw std::runtime_error("compact canonical present rejected");
    if(!runtime.undo()||!runtime.objects().empty()||!runtime.redo()||runtime.objects()!=objects)throw std::runtime_error("compact history identity changed");
    const auto firstReplay=runtime.brushReplayDigest();
    for(auto& s:batch.samples){s.pointer=2;s.sequence+=3;s.timestampNs+=40'000'000;s.y+=.0001F;}
    if(!runtime.acceptPlatformBatch(batch,batch.samples.back().timestampNs)||runtime.brushReplayDigest()==firstReplay)
        throw std::runtime_error("compact replay evidence ignores canonical centreline/pressure");
    if(!runtime.undo()||runtime.objects()!=objects)throw std::runtime_error("second compact stroke undo");
    foundation::InkBrushConfig laser;laser.family=foundation::InkFamily::kLaser;
    if(!runtime.configureInkBrush(laser))throw std::runtime_error("laser config rejected");
    for(auto& s:batch.samples){s.pointer=3;s.sequence+=3;s.timestampNs+=50'000'000;}
    if(!runtime.acceptPlatformBatch(batch,batch.samples.back().timestampNs)||!runtime.laserActive()||runtime.objects()!=objects)throw std::runtime_error("laser polluted canonical");
    runtime.advanceLaserPresentation(std::numeric_limits<std::uint64_t>::max());
    if(runtime.laserActive()||runtime.objects()!=objects)throw std::runtime_error("laser fade polluted canonical");
    for(auto& s:batch.samples){s.pointer=4;s.sequence+=3;s.timestampNs+=50'000'000;}
    if(!runtime.acceptPlatformBatch(batch,batch.samples.back().timestampNs)||!runtime.laserActive())throw std::runtime_error("laser restart");
    runtime.cancelAllPointers();
    if(runtime.laserActive())throw std::runtime_error("focus/source cancellation retains laser state");
    std::cout<<"compact runtime AddStroke/canonical/undo/redo closed\n";
}

int main(){for(const char* profile:{"round-regular","round-handdrawn","marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1"}){previewTint(profile);run(profile);}}
