#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <fstream>

using namespace canvas;
namespace {
constexpr unsigned width=256,height=128;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<std::uint8_t> draw(const foundation::InkBrushConfig& config,
                              const std::vector<foundation::InkKnot>& knots,float scale=1,float tx=0,float ty=0){
    render::RasterSkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"resize");
    auto frame=provider.acquire();frame.frame.surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    frame.frame.surface->getCanvas()->translate(tx,ty);frame.frame.surface->getCanvas()->scale(scale,scale);
    render::internal::drawCompactStrokeToSkCanvas(*frame.frame.surface->getCanvas(),config,knots);
    provider.release();std::vector<std::uint8_t> pixels(width*height*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"readback");return pixels;
}
unsigned alpha(const std::vector<std::uint8_t>& p,unsigned x,unsigned y){return p[(y*width+x)*4+3];}
std::uint64_t sum(const std::vector<std::uint8_t>& p){std::uint64_t s=0;for(unsigned i=3;i<p.size();i+=4)s+=p[i];return s;}
}
int main(int argc,char** argv){
    const std::string mode=argc>1?argv[1]:"marker";
    foundation::InkBrushConfig c;c.size=32;c.roundMode=foundation::InkRoundMode::kRegular;c.color={.1,.3,.8,1};
    const std::vector<foundation::InkKnot> knots{{32,64,32768},{224,64,32768}};
    if(mode=="visual"){
        // Same size, paint, pressure and path: distinction must come from
        // material, not the name or a wider default nib.
        const auto chalk=ink::BrushPackageCatalog{}.loadDefault("chalk-compact-v1",2);
        const auto water=ink::BrushPackageCatalog{}.loadDefault("watercolor-lite-v1",2);
        require(bool(chalk)&&bool(water),"material visual revision missing");
        c=*chalk.package.compactConfig;c.size=32;c.color={.1,.3,.8,1};c.opacity=1;
        auto dry=draw(c,knots);c=*water.package.compactConfig;c.size=32;c.color={.1,.3,.8,1};c.opacity=1;
        const auto wash=draw(c,knots);
        unsigned pores=0,wetHoles=0;double edge=0,body=0;
        for(unsigned y=58;y<70;++y)for(unsigned x=50;x<206;++x){pores+=alpha(dry,x,y)<30;wetHoles+=alpha(wash,x,y)<30;body+=alpha(wash,x,y);}
        for(unsigned y=49;y<52;++y)for(unsigned x=50;x<206;++x)edge+=alpha(wash,x,y);
        std::cout<<"chalk dry pores="<<pores<<" watercolor holes="<<wetHoles<<" edge/body="<<(edge/3)/(body/12)<<std::endl;
        require(pores>180,"chalk lacks fine dry gaps");
        require(wetHoles<10,"watercolor is porous like chalk");
        require(edge/(3*156)>body/(12*156)*1.25,"watercolor lacks an observable pigment rim");
        double chalkSum=0,waterSum=0,chalkSquares=0,waterSquares=0;
        for(unsigned y=58;y<70;++y)for(unsigned x=50;x<206;++x){const auto a=alpha(dry,x,y),b=alpha(wash,x,y);chalkSum+=a;waterSum+=b;chalkSquares+=a*a;waterSquares+=b*b;}
        const auto chalkVariance=chalkSquares/(12*156)-std::pow(chalkSum/(12*156),2);
        const auto waterVariance=waterSquares/(12*156)-std::pow(waterSum/(12*156),2);
        require(chalkVariance>waterVariance*1.25,"chalk/watercolor texture variance too similar");
    }else if(mode=="swatch"){
        require(argc>2,"swatch output path required");
        std::ofstream out(argv[2],std::ios::binary);out<<"P6\n512 768\n255\n";
        std::vector<std::vector<std::uint8_t>> rows;
        const std::vector<foundation::InkKnot> path{{20,90,32768},{56,36,40000},{94,80,34000},{128,38,40000},{166,82,34000},{206,42,40000},{234,64,32768}};
        for(auto profile:{"round-handdrawn","marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1","round-regular"})for(unsigned revision:{1,2}){
            auto config=*ink::BrushPackageCatalog{}.loadDefault(profile,revision).package.compactConfig;
            config.size=28;config.color={.12,.35,.7,1};
            auto package=ink::BrushPackageCatalog{}.loadDefault(profile,revision).package;
            package.compactConfig=config;package.compactConfig->pressureSource=foundation::InkPressureSource::kDevice;
            ink::BrushSession stroke(1,ink::resolveBrushState(package,0));require(stroke.begin(),"swatch session");
            ink::BrushPreviewDelta preview;
            for(unsigned i=0;i<path.size();++i){const auto& k=path[i];ink::BrushSample s{k.x,k.y,double(k.pressure)/65535,true,i+1,std::uint64_t(i+1)*10'000'000,true};
                require(stroke.append({&s,1},{},preview)==ink::BrushSessionError::kNone,"swatch append");}
            ink::BrushCommitIntent intent;require(stroke.seal(intent)==ink::BrushSessionError::kNone,"swatch seal");
            rows.push_back(draw(config,intent.knots));
        }
        for(unsigned row=0;row<6;++row)for(unsigned y=0;y<height;++y)for(unsigned col=0;col<2;++col)for(unsigned x=0;x<width;++x){
            const auto& p=rows[row*2+col];const auto i=(y*width+x)*4;const auto a=p[i+3]/255.;
            for(unsigned channel=0;channel<3;++channel)out.put(char(std::lround(p[i+channel]*a+255*(1-a))));
        }
    }else if(mode=="marker"){
        c.family=foundation::InkFamily::kMarker;c.markerAngle=0;
        auto narrow=draw(c,knots);c.markerAngle=1.5707963267948966;auto wide=draw(c,knots);
        require(sum(wide)>sum(narrow)*2,"marker angle does not change flat nib sweep");
        c.markerFlatness=.8;require(draw(c,knots)!=wide,"marker flatness ignored");
    }else if(mode=="chalk"||mode=="crayon"){
        c.family=foundation::InkFamily::kChalk;c.density=.2;const auto sparse=draw(c,knots);
        c.density=.9;const auto dense=draw(c,knots);
        require(sum(dense)>sum(sparse)*1.4,"chalk density does not affect coverage");
        c.family=foundation::InkFamily::kCrayon;const auto wax=draw(c,knots);
        require(wax!=dense,"crayon is only a chalk rename");c.deposition=.1;
        require(draw(c,knots)!=wax,"crayon deposition ignored");
    }else if(mode=="water"){
        c.family=foundation::InkFamily::kWatercolor;c.wetEdge=0;const auto dry=draw(c,knots);
        c.wetEdge=1;const auto wet=draw(c,knots);
        require(wet!=dry,"watercolor wet edge ignored");
        require(alpha(wet,128,78)>alpha(wet,128,64),"watercolor has no concentrated pigment edge");
        unsigned low=255,high=0;for(unsigned x=60;x<190;++x){low=std::min(low,alpha(wet,x,64));high=std::max(high,alpha(wet,x,64));}
        require(high>low+5,"watercolor paper/interior is uniform");
    }else if(mode=="parity"){
        for(const unsigned revision:{1U,2U})for(const auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker,foundation::InkFamily::kChalk,foundation::InkFamily::kCrayon,foundation::InkFamily::kWatercolor}){
            c=*ink::BrushPackageCatalog{}.loadDefault("round-regular",revision).package.compactConfig;
            c.family=family;c.opacity=.5;
            render::TransparentOverlaySkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"resize");
            render::SkiaRenderer renderer;render::PreviewSurfaceController preview(renderer,provider);
            require(preview.begin(1,1,1,provider.generation()),"preview begin");
            auto prefix=std::make_shared<ink::BrushPreviewDelta::Chunk>();prefix->knots=knots;
            auto tail=std::make_shared<ink::BrushPreviewDelta::Chunk>();tail->previous=prefix;tail->knots={knots.back()};
            ink::BrushPreviewDelta delta;delta.revision=1;delta.compactConfig=c;delta.compactChunks=tail;
            render::PreviewStyleOverride style;style.preferCapturedPaint=true;
            require(preview.update(delta)&&preview.renderIfDirty(style),"preview update");
            std::vector<std::uint8_t> pixels(width*height*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"readback");
            require(pixels==draw(c,knots),"preview differs from canonical material/paint");
            renderer.clearCompactCache();delta.revision=2;
            require(preview.update(delta)&&preview.renderIfDirty(style),"rebuild preview");
            std::vector<std::uint8_t> rebuilt(pixels.size());require(provider.readbackRgba(rebuilt).code==render::BackendSubmissionCode::kAccepted,"readback rebuilt");
            require(rebuilt==pixels,"cache eviction changes material pixels");
            // A transparent white prefix cannot be substituted for captured
            // paint; test transforms and generation with an independent draw.
            require(preview.updateViewport(1.5F,8.F,-12.F)&&preview.renderIfDirty(style),"viewport preview");
            require(provider.readbackRgba(rebuilt).code==render::BackendSubmissionCode::kAccepted&&rebuilt==draw(c,knots,1.5F,8.F,-12.F),"viewport preview/canonical mismatch");
            require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted&&preview.rebind(provider.generation())&&preview.renderIfDirty(style),"rebind preview");
            require(provider.readbackRgba(rebuilt).code==render::BackendSubmissionCode::kAccepted&&rebuilt==draw(c,knots,1.5F,8.F,-12.F),"generation rebind changed pixels");
            const auto submits=renderer.submissionCount();require(preview.renderIfDirty(style)&&renderer.submissionCount()==submits,"clean preview resubmitted");
            // The product's amber tint changes presentation paint only. Keep
            // the same pressure coverage and material, applying opacity once.
            delta.revision=3;
            render::PreviewStyleOverride amber;
            require(preview.update(delta,1.5F,8.F,-12.F)&&preview.renderIfDirty(amber),"amber preview");
            auto tinted=c;tinted.color={amber.red,amber.green,amber.blue,1};tinted.opacity=amber.opacityMultiplier;
            require(provider.readbackRgba(rebuilt).code==render::BackendSubmissionCode::kAccepted&&rebuilt==draw(tinted,knots,1.5F,8.F,-12.F),"amber preview paint/opacity differs from render style");
            require(preview.geometry().contours.front().compactConfig==c,"preview tint mutated captured material config");
        }
    }else if(mode=="sampling"){
        c.family=foundation::InkFamily::kChalk;
        auto sparse=draw(c,knots);std::vector<foundation::InkKnot> dense;
        for(unsigned i=32;i<=224;++i)dense.push_back({double(i),64,32768});
        require(draw(c,dense)==sparse,"event density darkens same material stroke");
        c.opacity=.5;auto translucent=draw(c,knots);c.opacity=1;auto opaque=draw(c,knots);
        for(unsigned x=40;x<216;++x)require(std::abs(int(alpha(translucent,x,64))*2-int(alpha(opaque,x,64)))<=2,"stroke opacity applied more than once");
        const std::vector<foundation::InkKnot> crossing{{40,30,32768},{210,98,32768},{40,98,32768},{210,30,32768}};
        auto image=draw(c,crossing);require(sum(image)>0,"self intersection vanished");
        const std::vector<foundation::InkKnot> duplicate{{32,64,32768},{224,64,32768},{32,64,32768},{224,64,32768}};
        require(draw(c,duplicate)==sparse,"same-stroke overlap accumulates opacity");
    }else if(mode=="cache"){
        render::TransparentOverlaySkiaSurfaceProvider provider;require(provider.resize(512,512).code==render::BackendSubmissionCode::kAccepted,"cache resize");
        render::SkiaRenderer renderer;render::PreviewSurfaceController preview(renderer,provider);
        render::PreviewStyleOverride style;style.preferCapturedPaint=true;
        for(unsigned i=0;i<128;++i){
            auto chunk=std::make_shared<ink::BrushPreviewDelta::Chunk>();
            const double y=i<32?double(i)*12+10:10000.+i*20;
            chunk->knots={{0,y,32768},{4096,y,32768}};
            ink::BrushPreviewDelta delta;delta.revision=i+1;delta.compactConfig=c;delta.compactConfig->family=foundation::InkFamily::kChalk;delta.compactChunks=chunk;
            require(preview.begin(1,i+1,1,provider.generation())&&preview.updateForSession(i+1,delta)&&preview.renderIfDirty(style),"128-object preview");
            const auto work=renderer.compactWork();require(work.retainedBytes<=32*1024*1024&&work.temporaryBytes<=16*1024*1024,"aggregate cache exceeds budget");
            require(preview.retireSession(i+1,provider.generation()),"preview retirement");
        }
        std::cout<<"cache_retained_bytes="<<renderer.compactWork().retainedBytes<<" temporary_bytes="<<renderer.compactWork().temporaryBytes<<'\n';
    }else if(mode=="scene-mask"){
        c.family=foundation::InkFamily::kChalk;
        semantic::ObjectRecord object;object.id=foundation::ObjectId::fromUint64(1);
        object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;
        object.content=semantic::CompactStrokeContent{{c,42,knots}};
        const auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
        require(projection.records.size()==1,"compact scene projection");
        const auto& bounds=projection.records.front().worldBounds;
        require(bounds.left<=16&&bounds.right>=240&&bounds.top<=48&&bounds.bottom>=80,"compact scene clipped pressure coverage");
        render::RasterSkiaSurfaceProvider provider;
        require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"mask surface");
        const render::FrameState frame{
            render::ViewId{1},render::CameraState{{0,0},1,0,render::CameraGeneration{1}},
            {0,0,width,height},{width,height,width,height,1,1},semantic::SemanticGeneration{1},
            foundation::SceneRevision{1},render::SurfaceGeneration{provider.generation()},
            render::MetricsGeneration{1},render::FrameId{1}};
        render::ReferenceDrawList list{frame};
        list.viewportClip={0,0,width,height};
        list.entries.push_back({projection.records.front(),true,render::CompactStrokeReferenceCommand{std::get<semantic::CompactStrokeContent>(object.content)}});
        semantic::FilledPathMask mask;
        mask.path.commands={semantic::MoveTo{{110,0}},semantic::LineTo{{146,0}},semantic::LineTo{{146,128}},semantic::LineTo{{110,128}},semantic::ClosePath{}};
        list.entries.front().record.eraseMasks={{foundation::ObjectId::fromUint64(2),mask}};
        const auto plan=render::FramePlanBuilder::build(frame,list,{},true,true);
        require(bool(plan),"compact frame plan");
        render::SkiaRenderer renderer;
        require(renderer.renderFrame(provider,plan.value()).code==render::BackendSubmissionCode::kAccepted,"compact mask frame submission");
        std::vector<std::uint8_t> pixels(width*height*4);
        require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"mask readback");
        require(alpha(pixels,128,64)==0&&alpha(pixels,80,64)>0,"compact erase mask did not preserve only unmasked pixels");
        std::cout<<"compact scene bounds + FramePlan + Skia mask accepted\n";
    }else if(mode=="presets"){
        for(const auto profile:{"marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1"}){
            const auto p=ink::BrushPackageCatalog{}.loadDefault(profile,1);
            require(bool(p)&&p.package.compactConfig.has_value(),"missing compact material preset");
        }
    }
    std::cout<<"InkV2Material "<<mode<<" passed\n";
}
