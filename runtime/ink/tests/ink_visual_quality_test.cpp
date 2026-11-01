#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace canvas;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<foundation::InkKnot> previewKnots(const ink::BrushPreviewDelta& preview){
    std::vector<const ink::BrushPreviewDelta::Chunk*> chunks;
    for(auto c=preview.compactChunks.get();c;c=c->previous.get())chunks.push_back(c);
    std::vector<foundation::InkKnot> result;
    for(auto c=chunks.rbegin();c!=chunks.rend();++c)for(auto k:(*c)->knots)
        if(result.empty()||result.back()!=k)result.push_back(k);
    return result;
}
}
int main(){
    ink::BrushPackageCatalog catalog;
    for(auto profile:{"round-regular","round-handdrawn","marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1"}){
        const auto old=catalog.loadDefault(profile,1),next=catalog.loadDefault(profile,2);
        require(bool(old)&&bool(next),"visual revision 2 missing");
        require(old.package.compactConfig->algorithmVersion==1&&next.package.compactConfig->algorithmVersion==2,"visual revisions share geometry interpretation");
        require(old.package.compactConfig->materialResourceSha256!=next.package.compactConfig->materialResourceSha256,"visual revision has no immutable material identity");
        auto state=ink::resolveBrushState(next.package,42);
        state.package.compactConfig->pressureSource=foundation::InkPressureSource::kDevice;
        ink::BrushSession singles(1,state),batch(2,state);require(singles.begin()&&batch.begin(),"session begin");
        // An explicit 90 degree corner. Quadratic midpoint fitting rounds the
        // bend inside the control polygon without overshoot or moving endpoints.
        std::vector<ink::BrushSample> samples{{20,80,.5,true,1,10'000'000,true},{60,80,.5,true,2,20'000'000,true},{60,40,.5,true,3,30'000'000,true},{100,40,.5,true,4,40'000'000,true}};
        ink::BrushPreviewDelta live,whole;
        for(auto s:samples)require(singles.append({&s,1},{},live)==ink::BrushSessionError::kNone,"single append");
        require(batch.append(samples,{},whole)==ink::BrushSessionError::kNone,"batch append");
        require(previewKnots(live)==previewKnots(whole),"batching changes fitted geometry");
        const auto points=previewKnots(live);bool rounded=false;
        for(auto p:points){require(p.x>=20&&p.x<=100&&p.y>=40&&p.y<=80,"curve overshoots control bounds");if(std::abs(p.x-55)<1&&std::abs(p.y-75)<1)rounded=true;}
        require(rounded,"corner remains a polyline instead of a midpoint quadratic");
        require(points.front().x==20&&points.front().y==80&&points.back().x==100&&points.back().y==40,"curve moved stroke endpoints");
        ink::BrushCommitIntent seal;require(singles.seal(seal)==ink::BrushSessionError::kNone&&seal.knots==points,"seal changes confirmed-only fitted preview");
        // Predictions can reshape the disposable tail, never persisted knots.
        ink::BrushSession predicted(3,state);require(predicted.begin(),"predicted begin");
        auto future=samples.back();future.sequence=5;future.timestampNs=50'000'000;future.x=200;
        require(predicted.append(samples,{&future,1},live)==ink::BrushSessionError::kNone,"predicted append");
        ink::BrushCommitIntent confirmed;require(predicted.seal(confirmed)==ink::BrushSessionError::kNone&&confirmed.knots==seal.knots,"prediction entered canonical geometry");
        ink::BrushSession longStroke(4,state);require(longStroke.begin(),"long begin");
        for(unsigned i=0;i<300;++i){ink::BrushSample s{double(i)*2,60+30*std::sin(i*.05),.5,true,i+1,std::uint64_t(i+1)*8'000'000,true};
            require(longStroke.append({&s,1},{},live)==ink::BrushSessionError::kNone,"long append");}
        const auto final=previewKnots(live);
        require(longStroke.seal(seal)==ink::BrushSessionError::kNone&&seal.knots==final,"chunk boundary changes sealed curve");
        for(unsigned count:{1U,2U}){
            ink::BrushSession shortStroke(5,state);require(shortStroke.begin(),"short begin");
            for(unsigned i=0;i<count;++i){ink::BrushSample s{20.+i,40,0,true,i+1,std::uint64_t(i+1)*8'000'000,true};
                require(shortStroke.append({&s,1},{},live)==ink::BrushSessionError::kNone,"zero pressure short input");}
            const auto shortPreview=previewKnots(live);
            require(shortStroke.seal(seal)==ink::BrushSessionError::kNone&&seal.knots==shortPreview&&seal.knots.back().x==19.+count,"short stroke fitting drops endpoint");
        }
    }
    std::cout<<"visual revision 2: quadratic corners, endpoints, batching, seal and prediction checks passed\n";
}
