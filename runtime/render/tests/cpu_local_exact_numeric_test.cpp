// Numeric design preflight. The candidate here is test-only until the original
// decision corpus qualifies it; it never substitutes for a production oracle.
#define main cpuw6bPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "cpu_local_exact_original.hpp"
#include "../src/cpu_local_exact.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "canvas/semantic/snapshot.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <limits>

namespace {
using json=nlohmann::json;
using Hit=SelectionGeometryHit;
std::vector<SkPath> pieces(const semantic::EraseMaskGeometry& geometry) {
    if(std::holds_alternative<semantic::FilledPathMask>(geometry))
        return {*cpuw6b_reference::cpuw6bSequentialPath(geometry,nullptr)};
    const auto& segments=std::get<semantic::SweptCircleMask>(geometry).segments;
    std::vector<SkPath> result;
    for(std::size_t first=0;first<segments.size();first+=32) {
        semantic::SweptCircleMask subset;
        subset.segments.assign(segments.begin()+first,segments.begin()+std::min(first+32,segments.size()));
        auto path=cpuw6b_reference::cpuw6bSequentialPath(subset,nullptr);
        require(path&&path->isFinite(),"numeric piece finite");result.push_back(*path);
    }
    return result;
}
[[maybe_unused]] Hit prototype(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& query) {
    // Full reference validation remains ahead of any local early success.
    SkPath visible;std::vector<SkPath> components;
    auto status=cpuw6b_reference::geometry(record,visible,components,false,nullptr);
    if(status!=Hit::kHit)return status;
    const auto valid=[](const semantic::EraseMaskGeometry& value){
        if(const auto* s=std::get_if<semantic::SweptCircleMask>(&value))for(const auto& p:s->segments){
            for(double v:{p.p0.position.x,p.p0.position.y,p.p0.radius,p.p1.position.x,p.p1.position.y,p.p1.radius,
                    p.control1.x,p.control1.y,p.control2.x,p.control2.y})if(!std::isfinite(v)||!std::isfinite(float(v)))return false;
            if(p.p0.radius<0||p.p1.radius<0)return false;
        }
        return true;
    };
    if(!valid(query))return Hit::kInvalid;
    for(const auto& mask:record.eraseMasks)if(!valid(mask.geometry))return Hit::kInvalid;
    components.insert(components.begin(),visible);
    std::vector<SkPath> history;
    for(const auto& mask:record.eraseMasks) {
        const auto paths=pieces(mask.geometry);history.insert(history.end(),paths.begin(),paths.end());
    }
    const auto parts=pieces(query);
    for(const auto& q:parts)for(const auto& body:components) {
        if(body.isEmpty()||!SkRect::Intersects(body.getBounds(),q.getBounds()))continue;
        auto overlap=Op(body,q,kIntersect_SkPathOp);if(!overlap)return Hit::kInvalid;
        for(const auto& old:history) {
            if(overlap->isEmpty())break;
            if(!SkRect::Intersects(overlap->getBounds(),old.getBounds()))continue;
            overlap=Op(*overlap,old,kDifference_SkPathOp);if(!overlap)return Hit::kInvalid;
        }
        if(!overlap->isEmpty())return Hit::kHit;
    }
    return Hit::kMiss;
}
Hit candidate(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& query) {
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});
    LocalExactCache cache;PreparedGeometryCache bodies;RenderCacheOptions options;
    options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;
    auto index=cache.request(query,budget,options.maxEntryBytes);require(bool(index),"query index");
    LocalExactJob job(record,index,cache,bodies,options,budget);
    for(unsigned turn=0;turn<20000;++turn){
        PreparationAllowance allowance;const auto status=job.advance(allowance);
        if(status==LocalExactStatus::kHit)return Hit::kHit;
        if(status==LocalExactStatus::kMiss)return Hit::kMiss;
        if(status==LocalExactStatus::kInvalid)return Hit::kInvalid;
        if(status==LocalExactStatus::kNeedFull)return cpuw6b_reference::eraseGeometryQuery(record,query,nullptr,false);
        if(status==LocalExactStatus::kNeedPiece)job.advanceLocalPiece(allowance);
    }
    throw std::runtime_error("production local numeric progress");
}
Hit localSelection(const RuntimeSceneRecord& record,const std::vector<foundation::WorldPoint>& points){
    SkiaRenderer renderer;RenderCacheOptions limits;limits.maxBytes=256U*1024U*1024U;limits.sharedMaxBytes=512U*1024U*1024U;limits.maxEntryBytes=64U*1024U*1024U;
    renderer.setRenderCacheOptions(limits);renderer.setCpuPendingPreparation(true);
    const auto floor=renderer.reserveCpuWorkerGrant(1,64U*1024U*1024U);require(floor,"selection budget");
    for(unsigned i=0;i<20000;++i){renderer.beginCpuPendingOpportunity();bool pending=false;
        const auto status=renderer.intersectSelectionGeometry(record,points,&pending);
        if(!pending){renderer.clearCpuLocalSelection();require(renderer.releaseCpuWorkerGrant(1,floor),"selection release");return status;}
    }
    throw std::runtime_error("selection no progress");
}
semantic::EraseMaskGeometry sweep(double x,double y,double endX,double endY,unsigned n,double radius,
        semantic::Transform2D transform={}) {
    std::vector<foundation::WorldPoint> trace;
    for(unsigned i=0;i<=n;++i)trace.push_back({float(x+(endX-x)*i/n),float(y+(endY-y)*i/n)});
    return *interaction::partialEraseGeometry(trace,transform,radius);
}
// The anchored wire scanner rejects nonfinite input before the original raw
// PathOps helper can reach its equality/no-op shortcut. Preserve that admission.
Hit originalAdmitted(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& query) {
    const auto valid=[](const semantic::EraseMaskGeometry& value){
        if(const auto* s=std::get_if<semantic::SweptCircleMask>(&value))for(const auto& p:s->segments){
            for(double v:{p.p0.position.x,p.p0.position.y,p.p0.radius,p.p1.position.x,p.p1.position.y,p.p1.radius,
                    p.control1.x,p.control1.y,p.control2.x,p.control2.y})if(!std::isfinite(v))return false;
            if(p.p0.radius<0||p.p1.radius<0)return false;
        }
        return true;
    };
    if(!valid(query))return Hit::kInvalid;
    for(const auto& mask:record.eraseMasks)if(!valid(mask.geometry))return Hit::kInvalid;
    return cpuw6b_reference::eraseGeometryQuery(record,query,nullptr,false);
}
json cornerContactRegression(){
    // A radius .01 dot lies outside the top-left corner. Its gap to the
    // polygon is .00012: outside the original round .0001 contact band,
    // inside the unintended miter corner (.0001*sqrt(2)).
    auto entry=source();auto record=entry.record;
    record.kind=semantic::ObjectKind::kVectorStroke;record.kindVersion=3;
    auto content=std::get<semantic::CompactStrokeContent>(record.content.get());
    content.stroke.config.size=.02;content.stroke.config.thinning=0;
    content.stroke.config.startCap=true;content.stroke.config.endCap=true;
    const auto center=-float((.01+.00012)/std::sqrt(2.));
    content.stroke.knots={{center,center,32768}};record.content=content;
    const std::vector<foundation::WorldPoint> region{{0,0},{1,0},{1,1},{0,1}};
    const auto original=cpuw6b_reference::intersectVisibleSelectionGeometry(record,region);
    const auto actual=localSelection(record,region);
    SkPath visible;std::vector<SkPath> components;
    const auto body=cpuw6b_reference::geometry(record,visible,components,false,nullptr);
    SkPathBuilder polygon;polygon.moveTo(0,0);polygon.lineTo(1,0);polygon.lineTo(1,1);polygon.lineTo(0,1);polygon.close();
    SkPaint miter;miter.setStyle(SkPaint::kStroke_Style);miter.setStrokeWidth(2e-4F);
    const auto oldCandidateBand=skpathutils::FillPathWithPaint(polygon.detach(),miter);
    bool miterHit=false;
    for(const auto& part:components){const auto overlap=Op(part,oldCandidateBand,kIntersect_SkPathOp);if(overlap&&!overlap->isEmpty())miterHit=true;}
    const bool fixtureQualified=body==Hit::kHit&&original==Hit::kMiss&&miterHit;
    return {{"case","round-selection-corner-contact"},{"center",center},{"radius",.01},
        {"original",unsigned(original)},{"candidate",unsigned(actual)},
        {"unintended_miter_hit",miterHit},{"fixture_qualified",fixtureQualified},
        {"equal",fixtureQualified&&actual==original}};
}
}
int main(int argc,char** argv) {try {
    require(argc==2||argc==3,"output [Fixed40]");json rows=json::array();unsigned mismatches=0;
    if(argc==3&&std::string_view(argv[2])=="--corner-contact"){
        const auto corner=cornerContactRegression();
        std::ofstream(argv[1])<<corner.dump(2)<<'\n';std::cout<<corner.dump()<<'\n';
        return corner.at("equal").get<bool>()?0:2;
    }
    for(unsigned family=0;family<2;++family)for(unsigned variant=0;variant<18;++variant) {
        auto entry=source();auto record=entry.record;record.kind=semantic::ObjectKind::kVectorStroke;record.kindVersion=3;
        auto content=std::get<semantic::CompactStrokeContent>(record.content.get());
        content.stroke.config.family=family?foundation::InkFamily::kMarker:foundation::InkFamily::kRound;
        content.stroke.config.markerAngle=.7;content.stroke.config.markerFlatness=.35;
        content.stroke.config.size=20;content.stroke.config.thinning=0;content.stroke.knots={{100,100,32768},{200,100,32768}};
        record.content=content;
        auto query=sweep(120,100,180,100,96,2);
        if(variant==1)query=sweep(120,160,180,160,96,2);
        if(variant==2)query=sweep(120,112,180,112,96,2);
        if(variant==3)query=sweep(120,std::nextafter(112.F,0.F),180,std::nextafter(112.F,0.F),96,2);
        if(variant==4)query=sweep(120,std::nextafter(112.F,200.F),180,std::nextafter(112.F,200.F),96,2);
        if(variant==5)query=sweep(150,100,150,100,96,2);
        if(variant>=6&&variant<=10)record.eraseMasks.push_back({ObjectId::fromUint64(1),query});
        if(variant==7)record.eraseMasks.push_back({ObjectId::fromUint64(2),sweep(130,100,170,100,128,4)});
        if(variant==8){record.eraseMasks.clear();record.eraseMasks.push_back({ObjectId::fromUint64(9),sweep(80,100,220,100,128,20)});}
        if(variant==9){record.eraseMasks.clear();record.eraseMasks.push_back({ObjectId::fromUint64(7),sweep(120,100,180,100,97,2)});}
        if(variant==10)std::get<semantic::SweptCircleMask>(query).segments.back().control1.x=std::numeric_limits<double>::quiet_NaN();
        if(variant==11){record.transform={1,0,0,1,.125,.25};query=sweep(120.125,100.25,180.125,100.25,96,2,record.transform);}
        if(variant==12){record.transform={1,0,0,1,15,15};query=sweep(135,115,195,115,96,2,record.transform);}
        if(variant==13){record.transform={0,2,-2,0,40,50};query=sweep(-160,290,-160,410,96,4,record.transform);}
        if(variant==14){std::vector<foundation::WorldPoint> trace;for(unsigned i=0;i<=128;++i)trace.push_back({float(150+40*std::sin(i*.17)),float(100+4*std::cos(i*.17))});query=*interaction::partialEraseGeometry(trace,{},2);}
        if(variant==15){auto c=content;c.stroke.knots={{150,100,32768}};record.content=c;query=sweep(150,100,150,100,1,2);}
        if(variant==16)record.eraseMasks.push_back({ObjectId::fromUint64(12),sweep(0,0,1,1,96,1)});
        if(variant==17){auto c=content;c.stroke.config.opacity=0;record.content=c;}
        // Queries supplied to the evaluator are object-local, as in production.
        const auto at=std::chrono::steady_clock::now();
        const auto expected=originalAdmitted(record,query);
        std::optional<Hit> actual;std::string error;
        try{actual=candidate(record,query);}catch(const std::exception& e){error=e.what();}
        const bool equal=actual&&*actual==expected&&(variant!=10||*actual==Hit::kInvalid);
        if(!equal)++mismatches;
        rows.push_back({{"family",family},{"variant",variant},{"original",unsigned(expected)},
            {"candidate",actual?json(unsigned(*actual)):json(nullptr)},{"equal",equal},{"error",error},
            {"wall_ns",std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-at).count()}});
        if(variant!=10&&variant!=13){
            const double x=variant%3==0?110.00005:variant%3==1?150:300;
            std::vector<foundation::WorldPoint> points{{float(x),99},{float(x+1),99},{float(x+1),101},{float(x),101}};
            for(auto& p:points){const double a=p.x,b=p.y;const auto& t=record.transform;p={float(t.a*a+t.c*b+t.tx),float(t.b*a+t.d*b+t.ty)};}
            const auto original=cpuw6b_reference::intersectVisibleSelectionGeometry(record,points);
            const auto selected=localSelection(record,points);const bool same=original==selected;if(!same)++mismatches;
            rows.push_back({{"family",family},{"variant",variant},{"consumer","selection-contact"},{"original",unsigned(original)},
                {"candidate",unsigned(selected)},{"equal",same}});
        }
    }
    if(argc==3){
        std::ifstream input(argv[2],std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
        const auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
        require(decoded.ok(),"Fixed40 decode");std::vector<double> offsets;
        for(const auto& object:decoded.snapshot->objects){
            if(std::find(offsets.begin(),offsets.end(),object.transform.tx)!=offsets.end())continue;
            offsets.push_back(object.transform.tx);
            RuntimeSceneRecord record;record.objectId=object.id;record.kind=object.kind;record.kindVersion=object.kind_version;
            record.transform=object.transform;record.content=object.content;record.properties=object.properties;
            for(const auto& mask:object.erase_masks)record.eraseMasks.push_back(mask);
            std::vector<foundation::WorldPoint> trace{{420,160}};
            for(unsigned i=1;i<=7200;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/7200)});
            const auto a=*interaction::partialEraseGeometry(trace,record.transform,16);
            for(unsigned phase=0;phase<2;++phase){
                const auto q=phase?*interaction::partialEraseGeometry(std::vector<foundation::WorldPoint>{{350,210},{360,230}},record.transform,16):a;
                const auto at=std::chrono::steady_clock::now();const auto expected=originalAdmitted(record,q);
                const auto oracleNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-at).count();
                const auto actual=candidate(record,q);const bool equal=actual==expected;if(!equal)++mismatches;
                rows.push_back({{"fixture","Fixed40"},{"offset",object.transform.tx},{"phase",phase?"B1-after-A":"A7200"},
                    {"original",unsigned(expected)},{"candidate",unsigned(actual)},{"equal",equal},{"oracle_ns",oracleNs}});
                std::ofstream(argv[1])<<json{{"rows",rows},{"mismatches",mismatches},{"partial",true}}.dump(2)<<'\n';
                if(!phase)record.eraseMasks.push_back({ObjectId::fromUint64(99999),a});
            }
        }
        require(offsets.size()==16,"all16 Fixed40 poses");
    }
    std::ofstream(argv[1])<<json{{"rows",rows},{"mismatches",mismatches}}.dump(2)<<'\n';
    std::cout<<json{{"cases",rows.size()},{"mismatches",mismatches}}.dump()<<'\n';
    return mismatches?2:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
