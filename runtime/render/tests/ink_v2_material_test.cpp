#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkPicture.h"
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
                              const std::vector<foundation::InkKnot>& knots,float scale=1,float tx=0,float ty=0,std::uint64_t seed=0){
    render::RasterSkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"resize");
    auto frame=provider.acquire();frame.frame.surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    frame.frame.surface->getCanvas()->translate(tx,ty);frame.frame.surface->getCanvas()->scale(scale,scale);
    render::internal::drawCompactStrokeToSkCanvas(*frame.frame.surface->getCanvas(),config,knots,seed);
    provider.release();std::vector<std::uint8_t> pixels(width*height*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"readback");return pixels;
}
unsigned alpha(const std::vector<std::uint8_t>& p,unsigned x,unsigned y){return p[(y*width+x)*4+3];}
std::uint64_t sum(const std::vector<std::uint8_t>& p){std::uint64_t s=0;for(unsigned i=3;i<p.size();i+=4)s+=p[i];return s;}
std::vector<std::uint8_t> coverage(const foundation::InkBrushConfig& config,std::span<const foundation::InkKnot> knots,std::size_t begin=0,std::size_t end=0,double distance=0,double total=-1){
    render::RasterSkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"coverage resize");
    auto frame=provider.acquire();auto* canvas=frame.frame.surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
    if(end)render::internal::drawCompactCoverageRangeToSkCanvas(*canvas,config,knots,begin,end,distance,total,42);
    else render::internal::drawCompactCoverageToSkCanvas(*canvas,config,knots,0,-1,42);
    provider.release();std::vector<std::uint8_t> pixels(width*height*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"coverage readback");return pixels;
}
std::vector<std::uint8_t> rangedStroke(const foundation::InkBrushConfig& config,
                                       std::span<const foundation::InkKnot> knots,
                                       std::size_t begin,std::size_t end,
                                       double distance,double total){
    render::RasterSkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"ranged stroke resize");
    auto frame=provider.acquire();auto* canvas=frame.frame.surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
    render::internal::CompactStrokeRange range{begin,end,distance,total,begin==0,end==knots.size()};
    render::internal::drawCompactStrokeToSkCanvas(*canvas,config,knots,42,nullptr,nullptr,nullptr,false,nullptr,&range);
    provider.release();std::vector<std::uint8_t> pixels(width*height*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"ranged stroke readback");return pixels;
}
std::vector<std::uint8_t> fullStroke(const foundation::InkBrushConfig& config,std::span<const foundation::InkKnot> knots){
    render::RasterSkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"full stroke resize");
    auto frame=provider.acquire();auto* canvas=frame.frame.surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
    render::internal::drawCompactStrokeToSkCanvas(*canvas,config,knots,42);
    provider.release();std::vector<std::uint8_t> pixels(width*height*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"full stroke readback");return pixels;
}
}
int main(int argc,char** argv){
    const std::string mode=argc>1?argv[1]:"marker";
    foundation::InkBrushConfig c;c.size=32;c.roundMode=foundation::InkRoundMode::kRegular;c.color={.1,.3,.8,1};
    const std::vector<foundation::InkKnot> knots{{32,64,32768},{224,64,32768}};
    if(mode=="chunk-range"){
        c=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",3).package.compactConfig;c.size=18;c.thinning=.65;c.startTaper=28;c.endTaper=46;
        std::vector<foundation::InkKnot> path;for(unsigned i=0;i<900;++i)path.push_back({8.+i*.24,64.+7.*std::sin(i*.035),static_cast<std::uint16_t>(18000+(i%180)*220)});
        const auto full=fullStroke(c,path);const std::size_t begin=256,end=770;const auto total=ink::compactLength(path);const auto start=ink::compactLength(std::span(path).first(begin));const auto part=rangedStroke(c,path,begin,end,start,total);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)if(x>=static_cast<unsigned>(path[512].x)&&x<=static_cast<unsigned>(path[767].x)){
            const auto index=(y*width+x)*4;
            if(full[index+3]!=part[index+3]){std::cerr<<"diff x="<<x<<" y="<<y<<" full="<<unsigned(full[index+3])<<" part="<<unsigned(part[index+3])<<"\n";require(false,"chunk range alpha differs from Direct oracle");}
        }
    }else if(mode=="drywet"){
        for(const auto profile:{"chalk-compact-v1","watercolor-lite-v1"}){
            const auto loaded=ink::BrushPackageCatalog{}.loadDefault(profile,3);
            require(bool(loaded),"pressure chalk/watercolor revision missing");
            c=*loaded.package.compactConfig;c.size=32;c.thinning=0;c.opacity=1;
            const auto light=draw(c,{{32,64,10000},{224,64,10000}});
            const auto firm=draw(c,{{32,64,55000},{224,64,55000}});
            require(sum(firm)>sum(light)*1.35,"pressure does not change material at fixed width");
            unsigned holes=0;double mean=0,variance=0,nearDifference=0,farDifference=0,edge=0,body=0;
            for(unsigned x=48;x<208;++x){for(unsigned y=58;y<70;++y){const auto a=alpha(firm,x,y);holes+=a<35;mean+=a;variance+=a*a;
                nearDifference+=std::abs(int(a)-int(alpha(firm,x+1,y)));farDifference+=std::abs(int(a)-int(alpha(firm,x+7,y)));}
                edge+=alpha(firm,x,50);body+=alpha(firm,x,64);}
            mean/=1920;variance=variance/1920-mean*mean;
            std::cout<<profile<<" pigment="<<sum(light)<<"/"<<sum(firm)<<" holes="<<holes<<" mean="<<mean<<" variance="<<variance<<" correlation="<<nearDifference/farDifference<<" edge/body="<<edge/body<<std::endl;
            if(c.family==foundation::InkFamily::kChalk){
                require(holes>80&&holes<900,"chalk needs clustered pores with continuous pigment");
                require(nearDifference<farDifference*.85,"chalk pores remain uncorrelated salt-and-pepper");
                require(variance>1200,"chalk lacks dry pigment contrast");
            }else{
                require(holes<5&&mean>45&&mean<170,"watercolor is opaque or has dry holes");
                require(variance>8&&variance<900,"watercolor lacks gentle concentration variation");
                require(edge<body*.95,"watercolor has a uniform dark hose rim");
            }
            if(c.family==foundation::InkFamily::kChalk){auto sparse=c,dense=c;sparse.density=.1;dense.density=.9;
                require(sum(draw(dense,knots))>sum(draw(sparse,knots))*1.5,"chalk density no longer controls paper contact");
            }else{std::vector<foundation::InkKnot> ramp;for(unsigned x=32;x<=224;x+=2)ramp.push_back({double(x),64,std::uint16_t(12000+(x-32)*42000/192)});
                const auto sparseRamp=draw(c,{{32,64,12000},{224,64,54000}}),denseRamp=draw(c,ramp);
                double error=0;for(unsigned y=56;y<72;++y)for(unsigned x=48;x<208;++x)error+=std::abs(int(alpha(sparseRamp,x,y))-int(alpha(denseRamp,x,y)));
                std::cout<<"watercolor pressure-ramp sample error="<<error/(16*160)<<std::endl;
                require(error/(16*160)<1.2,"pressure concentration forms segment-dependent patches");
                auto dry=c,wet=c;dry.wetEdge=0;wet.wetEdge=1;
                require(draw(dry,knots)!=draw(wet,knots),"watercolor wetEdge ignored");}
            const std::vector<foundation::InkKnot> retrace{{32,64,32768},{224,64,32768},{32,64,32768},{224,64,32768}};
            require(draw(c,retrace)==draw(c,knots),"self-overlap creates internal pigment ring");
            std::vector<foundation::InkKnot> dense;for(unsigned x=32;x<=224;x+=2)dense.push_back({double(x),64,32768});
            require(draw(c,knots,1,0,0,42)==draw(c,dense,1,0,0,42),"sample density changes material");
            require(draw(c,knots,1,0,0,42)!=draw(c,knots,1,0,0,43),"material ignores captured seed");
            auto half=c;half.opacity=.5;require(std::abs(double(sum(draw(half,knots)))/sum(draw(c,knots))-.5)<.02,"stroke opacity accumulated per segment");
        }
        if(argc>2){
            std::ofstream file(argv[2],std::ios::binary);file<<"P6\n512 512\n255\n";
            std::vector<std::vector<std::uint8_t>> swatches;
            for(const auto profile:{"chalk-compact-v1","watercolor-lite-v1"})for(unsigned row=0;row<2;++row)for(unsigned revision:{2U,3U}){
                auto package=ink::BrushPackageCatalog{}.loadDefault(profile,revision).package;auto& config=*package.compactConfig;config.size=32;
                config.color=config.family==foundation::InkFamily::kChalk?std::array<double,4>{.1,.32,.9,1}:std::array<double,4>{.8,.18,.25,1};
                config.pressureSource=foundation::InkPressureSource::kDevice;
                ink::BrushSession session(1,ink::resolveBrushState(package,42));require(session.begin(),"material swatch session");ink::BrushPreviewDelta delta;
                for(unsigned i=0;i<140;++i){ink::BrushSample input{20+i*1.55,row?64+std::sin(i*.055)*24:64,.25+.55*i/139,true,i+1,std::uint64_t(i)*8'000'000,true,i==139};
                    require(session.append({&input,1},{},delta)==ink::BrushSessionError::kNone,"material swatch input");}
                ink::BrushCommitIntent intent;require(session.seal(intent)==ink::BrushSessionError::kNone,"material swatch seal");swatches.push_back(draw(config,intent.knots,1,0,0,42));}
            for(unsigned row=0;row<4;++row)for(unsigned y=0;y<height;++y)for(unsigned col=0;col<2;++col)for(unsigned x=0;x<width;++x){
                const auto& p=swatches[row*2+col];const auto offset=(y*width+x)*4;
                for(unsigned channel=0;channel<3;++channel)file.put(char(std::min(255U,unsigned(p[offset+channel])+255-p[offset+3])));}
        }
    }else if(mode=="wax"||mode=="drywet-parity"){
        for(const auto profile:mode=="wax"?std::vector<const char*>{"crayon-wax-v1"}:std::vector<const char*>{"chalk-compact-v1","watercolor-lite-v1"}){
        const auto loaded=ink::BrushPackageCatalog{}.loadDefault(profile,3);
        require(bool(loaded),"pressure crayon revision missing");c=*loaded.package.compactConfig;
        if(mode=="wax")require(c.algorithmVersion==5&&c.signalVersion==3&&c.materialVersion==3,"crayon evaluator binding");
        else require(c.algorithmVersion==6&&c.signalVersion==3&&c.materialVersion==4,"drywet evaluator binding");
        c.size=24;c.thinning=0;c.opacity=1;
        require(ink::compactRadius(c,10000)==ink::compactRadius(c,55000),"pressure pigment fixture changed width");
        const auto light=draw(c,{{32,64,10000},{224,64,10000}});
        const auto firm=draw(c,{{32,64,55000},{224,64,55000}});
        require(sum(firm)>sum(light)*1.4,"pressure only changes width, not pigment");
        unsigned translucent=0,opaque=0;double center=0,edge=0;
        for(unsigned x=48;x<208;++x){for(unsigned y=54;y<74;++y){const auto a=alpha(firm,x,y);translucent+=a>10&&a<235;opaque+=a>245;}
            center+=alpha(firm,x,64);edge+=alpha(firm,x,74);}
        if(mode=="wax")require(translucent>2000&&opaque<100,"crayon remains a solid scraped ribbon");
        require(edge<center*.7&&edge>center*.015,"crayon lacks soft grain edge");
        auto translucentConfig=c;translucentConfig.opacity=.5;const auto halfOpacity=draw(translucentConfig,{{32,64,55000},{224,64,55000}});
        require(std::abs(double(sum(halfOpacity))/sum(firm)-.5)<.02,"whole-stroke opacity applied more than once");
        std::vector<foundation::InkKnot> sparse{{32,64,32768},{224,64,32768}},dense;
        for(unsigned i=0;i<=96;++i)dense.push_back({32.+i*2,64,32768});
        require(draw(c,sparse)==draw(c,dense),"sample count changes dry pigment");
        const auto pixels=draw(c,sparse);require(pixels==draw(c,sparse),"non-deterministic crayon");
        // One shared material path must remain identical through chunks,
        // seed, viewport and cache reconstruction.
        auto package=loaded.package;package.compactConfig=c;
        ink::BrushSession stroke(1,ink::resolveBrushState(package,42));require(stroke.begin(),"wax session");
        render::TransparentOverlaySkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"wax surface");
        render::SkiaRenderer renderer;render::PreviewSurfaceController preview(renderer,provider);
        require(preview.begin(1,1,1,provider.generation()),"wax preview begin");
        render::PreviewStyleOverride style;style.preferCapturedPaint=true;ink::BrushPreviewDelta delta;
        for(unsigned i=0;i<180;++i){ink::BrushSample sample{12+i*1.25,64+std::sin(i*.07)*16,.2+.6*i/179.,true,i+1,std::uint64_t(i)*8'000'000,true,i==179};
            require(stroke.append({&sample,1},{},delta)==ink::BrushSessionError::kNone&&preview.update(delta)&&preview.renderIfDirty(style),"wax live preview");}
        ink::BrushCommitIntent intent;require(stroke.seal(intent)==ink::BrushSessionError::kNone,"wax seal");
        std::vector<std::uint8_t> live(width*height*4);require(provider.readbackRgba(live).code==render::BackendSubmissionCode::kAccepted,"wax readback");
        require(live==draw(c,intent.knots,1,0,0,42),"wax preview/canonical mismatch");
        const auto submits=renderer.compactWork().generatedKnots;
        require(preview.renderIfDirty(style)&&renderer.compactWork().generatedKnots==submits,"no-op wax preview resubmits");
        renderer.clearCompactCache();delta.revision++;require(preview.update(delta)&&preview.renderIfDirty(style),"wax cache rebuild");
        std::vector<std::uint8_t> rebuilt(live.size());require(provider.readbackRgba(rebuilt).code==render::BackendSubmissionCode::kAccepted&&rebuilt==live,"wax cache changes pixels");
        require(preview.updateViewport(1.25F,7.F,-8.F)&&preview.renderIfDirty(style),"wax transform");
        require(provider.readbackRgba(rebuilt).code==render::BackendSubmissionCode::kAccepted&&rebuilt==draw(c,intent.knots,1.25F,7,-8,42),"wax viewport mismatch");
        require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted&&preview.rebind(provider.generation())&&preview.renderIfDirty(style),"wax rebind");
        require(provider.readbackRgba(rebuilt).code==render::BackendSubmissionCode::kAccepted&&rebuilt==draw(c,intent.knots,1.25F,7,-8,42),"wax rebind changed pixels");
        auto full=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));full->getCanvas()->clear(SK_ColorBLACK);
        auto ranged=SkSurfaces::Raster(full->imageInfo());ranged->getCanvas()->clear(SK_ColorBLACK);
        render::internal::drawCompactCoverageToSkCanvas(*full->getCanvas(),c,intent.knots,0,-1,42);
        double distance=0;
        for(std::size_t begin=0;begin<intent.knots.size();begin+=17){const auto end=std::min(begin+17,intent.knots.size());
            render::internal::drawCompactCoverageRangeToSkCanvas(*ranged->getCanvas(),c,intent.knots,begin,end,distance,-1,42);
            for(auto i=std::max(std::size_t(1),begin);i<end;++i)distance+=std::hypot(intent.knots[i].x-intent.knots[i-1].x,intent.knots[i].y-intent.knots[i-1].y);}
        std::vector<std::uint8_t> a(live.size()),b(live.size());
        require(full->readPixels(full->imageInfo(),a.data(),width*4,0,0)&&ranged->readPixels(ranged->imageInfo(),b.data(),width*4,0,0)&&a==b,"wax ranged coverage mismatch");
        // Canonical coverage pictures are recorded at world identity and
        // replayed at the camera scale. They must not capture a device AA size.
        SkPictureRecorder recorder;auto* recording=recorder.beginRecording(SkRect::MakeWH(256,128));
        auto fineConfig=c;fineConfig.size=4;
        render::internal::drawCompactCoverageToSkCanvas(*recording,fineConfig,intent.knots,0,-1,42);
        auto picture=recorder.finishRecordingAsPicture();
        auto recorded=SkSurfaces::Raster(full->imageInfo()),direct=SkSurfaces::Raster(full->imageInfo());
        for(const auto scale:{.5F,2.F}){
            recorded->getCanvas()->resetMatrix();recorded->getCanvas()->clear(SK_ColorBLACK);recorded->getCanvas()->scale(scale,scale);recorded->getCanvas()->drawPicture(picture);
            direct->getCanvas()->resetMatrix();direct->getCanvas()->clear(SK_ColorBLACK);direct->getCanvas()->scale(scale,scale);
            render::internal::drawCompactCoverageToSkCanvas(*direct->getCanvas(),fineConfig,intent.knots,0,-1,42);
            require(recorded->readPixels(recorded->imageInfo(),a.data(),width*4,0,0)&&direct->readPixels(direct->imageInfo(),b.data(),width*4,0,0)&&a==b,"recorded wax coverage changes with camera");}
        require(full->readPixels(full->imageInfo(),a.data(),width*4,0,0),"seed reference readback");
        auto seeded=SkSurfaces::Raster(full->imageInfo());seeded->getCanvas()->clear(SK_ColorBLACK);
        render::internal::drawCompactCoverageToSkCanvas(*seeded->getCanvas(),c,intent.knots,0,-1,43);
        require(seeded->readPixels(seeded->imageInfo(),b.data(),width*4,0,0)&&(c.family==foundation::InkFamily::kWatercolor||a!=b),"seed has no effect on pigment");
        auto tapered=c;tapered.startTaper=16;tapered.endTaper=24;
        const auto taperedLine=draw(tapered,sparse);
        if(mode=="wax")require(alpha(taperedLine,34,68)<alpha(pixels,34,68)&&alpha(taperedLine,220,68)<alpha(pixels,220,68),"wax taper ignored");
        else require(sum(taperedLine)<sum(pixels)*.96,"pressure material taper ignored");
        if(argc>2){
            std::vector<std::vector<std::uint8_t>> swatches;
            const std::vector<foundation::InkKnot> curve{{20,84,15000},{50,40,20000},{86,80,30000},{125,40,40000},{164,80,54000},{206,42,42000},{236,65,24000}};
            for(unsigned row=0;row<4;++row)for(unsigned revision:{2U,3U}){
                auto cfg=*ink::BrushPackageCatalog{}.loadDefault("crayon-wax-v1",revision).package.compactConfig;
                cfg.size=row==0?16:row==1?24:32;cfg.color={.75,.12,.16,1};
                const auto k=row==0?std::vector<foundation::InkKnot>{{20,64,16000},{236,64,52000}}:curve;
                auto pack=ink::BrushPackageCatalog{}.loadDefault("crayon-wax-v1",revision).package;pack.compactConfig=cfg;pack.compactConfig->pressureSource=foundation::InkPressureSource::kDevice;
                ink::BrushSession authored(1,ink::resolveBrushState(pack,42));require(authored.begin(),"wax swatch begin");ink::BrushPreviewDelta output;
                for(unsigned i=0;i<k.size();++i){const auto& point=k[i];ink::BrushSample input{point.x,point.y,point.pressure/65535.,true,i+1,std::uint64_t(i)*8'000'000,true,i+1==k.size()};
                    require(authored.append({&input,1},{},output)==ink::BrushSessionError::kNone,"wax swatch append");}
                ink::BrushCommitIntent sealed;require(authored.seal(sealed)==ink::BrushSessionError::kNone,"wax swatch seal");
                swatches.push_back(draw(cfg,sealed.knots,1,0,0,42));}
            std::ofstream file(argv[2],std::ios::binary);file<<"P6\n512 512\n255\n";
            for(unsigned row=0;row<4;++row)for(unsigned y=0;y<height;++y)for(unsigned col=0;col<2;++col)for(unsigned x=0;x<width;++x){
                const auto& p=swatches[row*2+col];const auto offset=(y*width+x)*4;
                for(unsigned channel=0;channel<3;++channel)file.put(char(std::min(255U,unsigned(p[offset+channel])+255-p[offset+3])));}
        }
        std::cout<<profile<<" light/firm="<<sum(light)<<"/"<<sum(firm)<<" translucent="<<translucent<<" edge/body="<<edge/center<<std::endl;}
    }else if(mode=="quality"){

        c=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        c.thinning=0;
        std::vector<foundation::InkKnot> jitter;
        for(unsigned i=0;i<100;++i)jitter.push_back({20+i*2.1,64+24*std::sin(i*.075)+1.5*std::sin(i*2.3),32768});
        const auto fitted=ink::compileCompactContour(c,jitter);
        require(fitted.patches.size()>3,"quadratic outline missing");
        for(std::size_t i=1;i<fitted.patches.size();++i){const auto& a=fitted.patches[i-1];const auto& b=fitted.patches[i];
            for(const auto sides:{std::array{a.leftControl,a.leftEnd,b.leftStart,b.leftControl},std::array{a.rightControl,a.rightEnd,b.rightStart,b.rightControl}}){
                const auto u=ink::ContourPoint{sides[1].x-sides[0].x,sides[1].y-sides[0].y};
                const auto v=ink::ContourPoint{sides[3].x-sides[2].x,sides[3].y-sides[2].y};
                require(sides[1]==sides[2],"outline has a disconnected boundary");
                if(std::abs(u.x*v.y-u.y*v.x)>=1e-8){
                    // A round node can join the two one-sided contours at a
                    // sharp reversal. Smooth spans still require tangent parity;
                    // non-C1 boundaries must be inside an actual connected node.
                    bool roundNode=false;for(const auto& cap:fitted.caps)
                        roundNode=roundNode||std::hypot(sides[1].x-cap.x,sides[1].y-cap.y)<=cap.radius+1e-8;
                    require(roundNode,"smooth outline has tangent discontinuity outside a round join");
                }}}
        const auto image=draw(c,jitter);double roughness=0;std::vector<double> edge;
        for(unsigned x=35;x<210;++x){unsigned first=0;for(unsigned y=15;y<110;++y)if(alpha(image,x,y)>=128){first=y;break;}
            const auto a=first?alpha(image,x,first-1):0,b=alpha(image,x,first);edge.push_back(first-1+(127.5-a)/std::max(1.,double(b)-a));}
        for(std::size_t i=1;i+1<edge.size();++i)roughness+=std::abs(edge[i+1]-2*edge[i]+edge[i-1]);
        std::cout<<"quality edge roughness="<<roughness<<std::endl;
        // Compare to the actual independent pinned contour at this exact scale;
        // pixel edge quantization itself contributes to second differences.
        std::vector<ink::reference::VectorStrokeInput> raw;
        for(auto k:jitter)raw.push_back({k.x,k.y,.5});
        ink::reference::StrokeOptions opts;opts.size=16;opts.thinning=0;opts.streamline=0;opts.smoothing=.61;opts.simulate_pressure=false;opts.last=true;
        const auto contour=ink::reference::getStrokeOutlinePoints(ink::reference::getStrokePoints(raw,opts),opts);
        auto oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));oracle->getCanvas()->clear(SK_ColorTRANSPARENT);
        SkPathBuilder reference;reference.moveTo(contour[0].x,contour[0].y);
        ink::reference::StrokeOutlinePoint control=contour[1],last{(contour[1].x+contour[2].x)*.5,(contour[1].y+contour[2].y)*.5};
        reference.quadTo(control.x,control.y,last.x,last.y);
        for(std::size_t i=2;i+1<contour.size();++i){control={2*last.x-control.x,2*last.y-control.y};last={(contour[i].x+contour[i+1].x)*.5,(contour[i].y+contour[i+1].y)*.5};reference.quadTo(control.x,control.y,last.x,last.y);}
        reference.close();SkPaint paint;paint.setAntiAlias(true);paint.setColor(SK_ColorBLACK);oracle->getCanvas()->drawPath(reference.detach(),paint);
        std::vector<std::uint8_t> oraclePixels(width*height*4);require(oracle->readPixels(oracle->imageInfo(),oraclePixels.data(),width*4,0,0),"oracle readback");
        std::vector<double> oracleEdge;
        for(unsigned x=35;x<210;++x){unsigned first=0;for(unsigned y=15;y<110;++y)if(alpha(oraclePixels,x,y)>=128){first=y;break;}
            const auto a=first?alpha(oraclePixels,x,first-1):0,b=alpha(oraclePixels,x,first);oracleEdge.push_back(first-1+(127.5-a)/std::max(1.,double(b)-a));}
        double oracleRoughness=0;for(std::size_t i=1;i+1<oracleEdge.size();++i)oracleRoughness+=std::abs(oracleEdge[i+1]-2*oracleEdge[i]+oracleEdge[i-1]);
        std::cout<<"oracle roughness="<<oracleRoughness<<std::endl;
        require(roughness<oracleRoughness*1.15,"continuous outline rougher than reference");
        for(bool tapered:{false,true})for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker}){
            auto package=ink::BrushPackageCatalog{}.loadDefault(family==foundation::InkFamily::kRound?"round-handdrawn":"marker-compact-v1",4).package;
            c=*package.compactConfig;c.startTaper=tapered?8:0;c.endTaper=tapered?24:0;package.compactConfig=c;
            ink::BrushSession session(1,ink::resolveBrushState(package,42));require(session.begin(),"quality begin");
            render::TransparentOverlaySkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"quality resize");
            render::SkiaRenderer renderer;render::PreviewSurfaceController preview(renderer,provider);require(preview.begin(1,1,1,provider.generation()),"quality preview begin");
            render::PreviewStyleOverride style;style.preferCapturedPaint=true;ink::BrushPreviewDelta delta;
            for(unsigned i=0;i<180;++i){ink::BrushSample sample{10+i*1.25,64+std::sin(i*.07)*16,0,false,i+1,std::uint64_t(i)*8'000'000,false,i==179};
                require(session.append({&sample,1},{},delta)==ink::BrushSessionError::kNone&&preview.update(delta)&&preview.renderIfDirty(style),"quality preview");}
            ink::BrushCommitIntent intent;require(session.seal(intent)==ink::BrushSessionError::kNone,"quality seal");
            std::vector<std::uint8_t> pixels(width*height*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"quality pixels");
            const auto expected=draw(c,intent.knots);unsigned different=0,maxDifference=0;for(std::size_t i=3;i<pixels.size();i+=4){auto d=unsigned(std::abs(int(pixels[i])-int(expected[i])));different+=d>2;maxDifference=std::max(maxDifference,d);}
            std::cout<<"quality preview diff="<<different<<" max="<<maxDifference<<std::endl;
            require(different==0,"quality preview/canonical geometry mismatch");
        }

        c=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.thinning=0;c.startTaper=8;c.endTaper=24;
        const auto sparse=draw(c,{{20,64,32768},{220,64,32768}});
        require(alpha(sparse,100,67)>128,"sparse tapered path loses full-width middle");
        c=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
        std::vector<foundation::InkKnot> crossing{{20,30,20000},{80,95,36000},{150,30,28000},{80,95,42000},{220,50,22000},{40,55,32000}};
        auto full=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));full->getCanvas()->clear(SK_ColorBLACK);
        auto ranged=SkSurfaces::Raster(full->imageInfo());ranged->getCanvas()->clear(SK_ColorBLACK);
        render::internal::drawCompactCoverageToSkCanvas(*full->getCanvas(),c,crossing);
        ink::CompactContourCompiler compiler(c);const auto total=ink::compactLength(crossing);
        for(std::size_t begin=0;begin<crossing.size();begin+=2){ink::CompactContour emitted;const auto end=std::min(begin+2,crossing.size());
            compiler.append(std::span(crossing).subspan(begin,end-begin),total,emitted);
            if(end==crossing.size())compiler.finish(total,emitted);
            render::internal::drawCompactContourCoverageToSkCanvas(*ranged->getCanvas(),c,emitted);}
        std::vector<std::uint8_t> a(width*height*4),b(a.size());
        require(full->readPixels(full->imageInfo(),a.data(),width*4,0,0)&&ranged->readPixels(ranged->imageInfo(),b.data(),width*4,0,0),"range readback");
        for(std::size_t i=0;i<a.size();++i)require(std::abs(int(a[i])-int(b[i]))<=2,"stateful contour range differs at turn");
    }else if(mode=="tuning"){
        c=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",3).package.compactConfig;
        c.size=24;c.thinning=0;c.startTaper=0;c.endTaper=32;
        const std::vector<foundation::InkKnot> line{{32,64,32768},{224,64,32768}};
        const auto tapered=draw(c,line);
        require(alpha(tapered,230,64)==0,"taper leaves a terminal disk");
        require(alpha(tapered,200,70)>alpha(tapered,216,70),"taper does not progressively narrow");
        c.endTaper=0;c.startCap=false;c.endCap=false;
        auto butt=draw(c,line);require(alpha(butt,27,64)==0&&alpha(butt,229,64)==0,"cap disabled still rounded");
        c.startCap=true;c.endCap=true;auto round=draw(c,line);
        require(alpha(round,27,64)>0&&alpha(round,229,64)>0,"round caps missing");
        for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker}){
            auto package=ink::BrushPackageCatalog{}.loadDefault(family==foundation::InkFamily::kRound?"round-handdrawn":"marker-compact-v1",3).package;
            c=*package.compactConfig;c.endTaper=32;c.startTaper=8;package.compactConfig=c;
            ink::BrushSession session(1,ink::resolveBrushState(package,42));require(session.begin(),"tuned preview begin");
            render::TransparentOverlaySkiaSurfaceProvider provider;require(provider.resize(width,height).code==render::BackendSubmissionCode::kAccepted,"tuned provider resize");
            render::SkiaRenderer renderer;render::PreviewSurfaceController preview(renderer,provider);
            require(preview.begin(1,1,1,provider.generation()),"tuned controller begin");
            ink::BrushPreviewDelta delta;render::PreviewStyleOverride style;style.preferCapturedPaint=true;
            for(unsigned i=0;i<150;++i){const ink::BrushSample s{10+i*1.5,64+std::sin(i*.07)*12,0,false,i+1,std::uint64_t(i)*8'000'000,false};
                require(session.append({&s,1},{},delta)==ink::BrushSessionError::kNone&&preview.update(delta)&&preview.renderIfDirty(style),"tuned preview update");}
            ink::BrushCommitIntent intent;require(session.seal(intent)==ink::BrushSessionError::kNone,"tuned preview seal");
            std::vector<std::uint8_t> pixels(width*height*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"tuned provider readback");
            require(pixels==draw(c,intent.knots),"tapered chunk preview differs from canonical");
            const auto submits=renderer.submissionCount();require(preview.renderIfDirty(style)&&renderer.submissionCount()==submits,"unchanged tuning preview resubmitted");
        }
        // High thinning creates subpixel tails; analytic coverage must carry
        // fractional pixel alpha rather than drop a fine diagonal between rows.
        auto hairline=c;hairline.thinning=1;hairline.startTaper=hairline.endTaper=0;
        const std::vector<foundation::InkKnot> fine{{32,40,256},{220,87,256}};
        const auto finePixels=draw(hairline,fine);
        for(unsigned x=40;x<210;++x){unsigned coverage=0;const auto y=40.+(x-32)*.25;
            for(unsigned row=unsigned(y)-1;row<=unsigned(y)+1;++row)coverage+=alpha(finePixels,x,row);
            require(coverage>4,"subpixel diagonal has gaps");}
        // The navigation tile scheduler resumes by ranges. Tuning and taper
        // must preserve the same coverage there as direct and preview rendering.
        c=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",3).package.compactConfig;
        c.startTaper=8;c.endTaper=32;
        std::vector<foundation::InkKnot> path;
        for(unsigned i=0;i<130;++i)path.push_back({20.+i*1.5,64.+std::sin(i*.1)*10,std::uint16_t(20000+i*200)});
        auto full=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));
        auto ranged=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));
        full->getCanvas()->clear(SK_ColorBLACK);ranged->getCanvas()->clear(SK_ColorBLACK);
        render::internal::drawCompactCoverageToSkCanvas(*full->getCanvas(),c,path);
        for(std::size_t begin=0;begin<path.size();begin+=17)
            render::internal::drawCompactCoverageRangeToSkCanvas(*ranged->getCanvas(),c,path,begin,begin+17);
        std::vector<std::uint8_t> a(width*height*4),b(a.size());
        require(full->readPixels(full->imageInfo(),a.data(),width*4,0,0)&&ranged->readPixels(ranged->imageInfo(),b.data(),width*4,0,0)&&a==b,"tuned ranged coverage changed geometry");
    }else if(mode=="visual"){
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
        // Resuming coverage must keep the predecessor at the range boundary
        // and max-combine overlap exactly once, including material opacity.
        std::vector<foundation::InkKnot> path;
        for(unsigned i=0;i<130;++i)path.push_back({20.+i*1.5,40.+std::sin(i*.2)*20,std::uint16_t(20000+i*200)});
        auto full=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));
        auto ranged=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));
        full->getCanvas()->clear(SK_ColorBLACK);ranged->getCanvas()->clear(SK_ColorBLACK);
        render::internal::drawCompactCoverageToSkCanvas(*full->getCanvas(),c,path);
        for(std::size_t begin=0;begin<path.size();begin+=17)
            render::internal::drawCompactCoverageRangeToSkCanvas(*ranged->getCanvas(),c,path,begin,begin+17);
        std::vector<std::uint8_t> fullPixels(width*height*4),rangePixels(fullPixels.size());
        require(full->readPixels(full->imageInfo(),fullPixels.data(),width*4,0,0)&&
                ranged->readPixels(ranged->imageInfo(),rangePixels.data(),width*4,0,0)&&
                fullPixels==rangePixels,"resumed coverage differs from full authority");
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
    }else if(mode=="scene-mask"||mode=="wax-mask"||mode=="drywet-mask"){
        for(const auto profile:mode=="drywet-mask"?std::vector<const char*>{"chalk-compact-v1","watercolor-lite-v1"}:std::vector<const char*>{"crayon-wax-v1"}){
        if(mode=="drywet-mask"){c=*ink::BrushPackageCatalog{}.loadDefault(profile,3).package.compactConfig;c.size=32;}
        else if(mode=="wax-mask"){c=*ink::BrushPackageCatalog{}.loadDefault("crayon-wax-v1",3).package.compactConfig;c.size=32;}
        else c.family=foundation::InkFamily::kChalk;
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
        std::cout<<"compact scene bounds + FramePlan + Skia mask accepted\n";}
    }else if(mode=="presets"){
        for(const auto profile:{"marker-compact-v1","chalk-compact-v1","crayon-wax-v1","watercolor-lite-v1"}){
            const auto p=ink::BrushPackageCatalog{}.loadDefault(profile,1);
            require(bool(p)&&p.package.compactConfig.has_value(),"missing compact material preset");
        }
    }
    std::cout<<"InkV2Material "<<mode<<" passed\n";
}
