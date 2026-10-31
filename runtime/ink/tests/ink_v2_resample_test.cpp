#include "canvas/ink/compact_brush_session.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace canvas;
std::vector<foundation::InkKnot> line(unsigned hz){
    ink::CompactBrushSession session;
    for(unsigned i=0;i<=hz*2;++i){ink::TimedBrushSample s{double(i)*100/hz,0,0,false,i+1,std::uint64_t(i)*1'000'000'000/hz};if(!session.append({&s,1},{}))throw std::runtime_error("line append");}
    return session.knots();
}
int main(){
    const auto a=line(120),b=line(240);
    std::cout<<"retained 120Hz="<<a.size()<<" 240Hz="<<b.size()<<'\n';
    // Canonical record has a fixed config/identity envelope in addition to
    // eighteen bytes per knot. The contract limits encoded bytes, not count.
    if(a.size()>30||b.size()>30||std::abs(double(a.size())-b.size())*18>(250+18*std::min(a.size(),b.size()))*.15)throw std::runtime_error("sample-rate dependent compact carrier");
    if(a.front().x!=0||a.back().x!=200||b.back().x!=200)throw std::runtime_error("line endpoints lost");
    auto package=ink::BrushPackageCatalog{}.loadDefault("chalk-compact-v1",1).package;
    ink::BrushSession session(1,ink::resolveBrushState(package,42));session.begin();
    ink::BrushPreviewDelta preview;
    for(unsigned i=0;i<300;++i){const ink::BrushSample sample{double(i)*2,30*std::sin(double(i)/30),0,false,i+1,std::uint64_t(i)*8'000'000,false};
        if(session.append({&sample,1},{},preview)!=ink::BrushSessionError::kNone)throw std::runtime_error("curve input");}
    std::vector<const ink::BrushPreviewDelta::Chunk*> chunks;
    for(auto chunk=preview.compactChunks.get();chunk;chunk=chunk->previous.get())chunks.push_back(chunk);
    std::vector<foundation::InkKnot> finalPreview;
    for(auto c=chunks.rbegin();c!=chunks.rend();++c)for(const auto& k:(*c)->knots)
        if(finalPreview.empty()||finalPreview.back()!=k)finalPreview.push_back(k);
    ink::BrushCommitIntent intent;if(session.seal(intent)!=ink::BrushSessionError::kNone)throw std::runtime_error("curve seal");
    std::cout<<"preview_knots="<<finalPreview.size()<<" sealed_knots="<<intent.knots.size()<<"\n";
    if(intent.knots!=finalPreview)throw std::runtime_error("seal changes confirmed-only preview geometry");
    ink::BrushSession cancelled(2,ink::resolveBrushState(package,43));
    const ink::BrushSample oldPoint{20,30,0,false,1,1,false};
    const ink::BrushSample freshPoint{200,300,0,false,1,1,false};
    if(!cancelled.begin()||cancelled.append({&oldPoint,1},{},preview)!=ink::BrushSessionError::kNone)
        throw std::runtime_error("cancel fixture begin");
    cancelled.cancel();
    if(!cancelled.begin()||cancelled.append({&freshPoint,1},{},preview)!=ink::BrushSessionError::kNone||
       cancelled.seal(intent)!=ink::BrushSessionError::kNone||intent.knots.size()!=1||intent.knots.front().x!=200)
        throw std::runtime_error("cancelled compact history survives session restart");
    // Independent pressure formula and geometric projection; does not call
    // production simplification/radius helpers to derive expected values.
    for(const unsigned hz:{60U,120U,240U})for(const unsigned batchSize:{1U,8U,31U}){
        std::vector<ink::TimedBrushSample> raw;
        std::vector<double> pressure;
        double p=.5,px=0,py=0;std::uint64_t previous=0;
        for(unsigned i=0;i<=hz*2;++i){
            const double t=double(i)/hz,x=100*t,y=25*std::sin(t*3.141592653589793);
            const auto ns=std::uint64_t(i)*1'000'000'000/hz;
            if(i){const auto dt=double(ns-previous)/1e9,v=std::hypot(x-px,y-py)/dt;
                const auto u=std::clamp((v-80.)/1120.,0.,1.);p+=(.8-.6*u*u*(3.-2.*u)-p)*(1.-std::exp(-dt/.032));}
            raw.push_back({x,y,.99,false,i+1,ns,false});pressure.push_back(p);px=x;py=y;previous=ns;
        }
        ink::CompactBrushSession observed;
        for(std::size_t i=0;i<raw.size();i+=batchSize)if(!observed.append({raw.data()+i,std::min<std::size_t>(batchSize,raw.size()-i)},{}))throw std::runtime_error("oracle ingress");
        const auto compact=observed.knots();double maxGeometry=0,maxPressure=0;
        for(std::size_t i=0;i<raw.size();++i){double best=1e30,at=0;
            for(std::size_t k=1;k<compact.size();++k){const auto& l=compact[k-1];const auto& r=compact[k];const auto dx=r.x-l.x,dy=r.y-l.y;
                const auto q=std::clamp(((raw[i].x-l.x)*dx+(raw[i].y-l.y)*dy)/(dx*dx+dy*dy),0.,1.);
                const auto d=std::hypot(raw[i].x-l.x-q*dx,raw[i].y-l.y-q*dy);
                if(d<best){best=d;at=(l.pressure+(double(r.pressure)-l.pressure)*q)/65535.;}}
            maxGeometry=std::max(maxGeometry,best);maxPressure=std::max(maxPressure,std::abs(at-pressure[i]));
        }
        if(maxGeometry>.25||maxPressure>.02)throw std::runtime_error("compact exceeds independent geometry/pressure error budget");
        std::cout<<"hz="<<hz<<" batch="<<batchSize<<" knots="<<compact.size()<<" max_geometry_error="<<maxGeometry<<" max_pressure_error="<<maxPressure<<'\n';
    }
}
