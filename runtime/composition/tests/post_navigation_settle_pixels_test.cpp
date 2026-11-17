#define main priorCanonicalMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "../../render/src/skia_ink_layers.hpp"
#include "../../render/src/skia_navigation_tiles.hpp"
#include "include/core/SkColorSpace.h"
#include <nlohmann/json.hpp>
namespace canvas::render::internal {struct ContinuousZoomProbe {
 static bool completeRegion(const SkiaNavigationTiles&n){for(const auto&[key,tile]:n.tiles_)if(key.region&&tile.image)return tile.valid.full(n.grid_.bounds(key));return false;}
};}
int main(int argc,char**argv){
 require(argc==2,"pixel output");auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"pixelMetal");
 auto read=[](SkSurface&surface){auto info=SkImageInfo::MakeN32Premul(surface.width(),surface.height());std::vector<std::uint8_t> out(info.computeMinByteSize());require(surface.readPixels(info,out.data(),info.minRowBytes(),0,0),"pixelRead");return out;};
 auto gpu=[&](int w,int h){return SkSurfaces::RenderTarget(completion->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(w,h));};
 auto e=source();auto c=std::get<CompactStrokeReferenceCommand>(e.command).content.get();c.stroke.config.size=9;c.stroke.config.opacity=.4;
 c.stroke.knots.clear();for(unsigned i=0;i<200;++i)c.stroke.knots.push_back({80+55*std::sin(i*.1),75+50*std::sin(i*.17),std::uint16_t(25000+i*90)});
 e.record.content=c;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
 RenderCacheOptions options;options.maxBytes=512U*1024*1024;options.sharedMaxBytes=1024U*1024*1024;options.maxEntryBytes=128U*1024*1024;auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});PreparedGeometryCache prepared;auto body=prepared.get(e,options,budget,true);require(body&&body->ready(),"pixelPrepared");
 SkiaInkLayers layers;CompactCoverageTargets targets;targets.budget=budget;RenderCacheWork work;nlohmann::json cases=nlohmann::json::array();
 for(unsigned variant=0;variant<4;++variant){
  SkMatrix matrix=SkMatrix::MakeAll(variant==3?-1.1F:1.3F,variant==2?.2F:0,variant==3?230:12,variant==2?.15F:0,1.2F,9,0,0,1);
  auto scratch=gpu(256,256);sk_sp<SkImage> white;SkMatrix basis;for(unsigned i=0;;++i){PreparationAllowance a;auto r=layers.prepareNavigationBody(e,body,matrix,*scratch->getCanvas(),options,budget,targets,work,a,white,basis);if(r==SkiaInkLayers::BodyPreparation::kReady)break;require(r==SkiaInkLayers::BodyPreparation::kPending&&i<128,"pixelBodyProgress");}
  auto actual=gpu(256,256),reference=gpu(256,256);for(auto*s:{actual.get(),reference.get()}){auto*d=s->getCanvas();d->clear(SK_ColorWHITE);SkPaint shape;shape.setColor(0xff306090);d->drawRect(SkRect::MakeXYWH(15,25,95,150),shape);}
  for(unsigned copy=0;copy<2;++copy){
   auto coverage=gpu(256,256),oracle=gpu(256,256);for(auto*s:{coverage.get(),oracle.get()}){s->getCanvas()->clear(SK_ColorBLACK);s->getCanvas()->setMatrix(matrix);}
   coverage->getCanvas()->resetMatrix();coverage->getCanvas()->drawImage(white,float(std::round(matrix.getTranslateX()-basis.getTranslateX())),float(std::round(matrix.getTranslateY()-basis.getTranslateY())));coverage->getCanvas()->setMatrix(matrix);body->drawContour(*oracle->getCanvas());
   SkPaint erase;erase.setBlendMode(SkBlendMode::kClear);erase.setAntiAlias(false);for(auto*s:{coverage.get(),oracle.get()})s->getCanvas()->drawRect(SkRect::MakeXYWH(copy?100:50,25,12,110),erase);
   for(const auto pair:{std::pair{actual.get(),coverage.get()},std::pair{reference.get(),oracle.get()}}){auto*d=pair.first->getCanvas();d->save();d->setMatrix(matrix);compositeCompactCoverageToSkCanvas(*d,*pair.second->makeImageSnapshot(),c.stroke.config,c.stroke.seed);d->restore();}
   if(!copy)for(auto*s:{actual.get(),reference.get()}){SkPaint p;p.setColor(0x8050a040);s->getCanvas()->drawRect(SkRect::MakeXYWH(80,80,80,55),p);}
  }
  auto edge=SkSurfaces::Raster(SkImageInfo::MakeA8(256,256));edge->getCanvas()->clear(SK_ColorTRANSPARENT);SkPaint boundary;boundary.setColor(SK_ColorWHITE);boundary.setStyle(SkPaint::kStroke_Style);boundary.setStrokeWidth(2);boundary.setAntiAlias(true);
  const auto drawEdge=[&](const SkPath&p){edge->getCanvas()->drawPath(p.makeTransform(matrix),boundary);};
  const auto contour=ink::compileCompactContour(c.stroke.config,c.stroke.knots);
  for(std::size_t j=0;j+1<contour.patches.size();++j)drawEdge(compactContourRibbon(std::span(contour.patches).subspan(j,2)));
  for(const auto&cap:contour.caps){SkPathBuilder p;p.addCircle(float(cap.x),float(cap.y),float(cap.radius));drawEdge(p.detach());}
  for(float x:{50.F,100.F}){SkPathBuilder p;p.addRect(SkRect::MakeXYWH(x,25,12,110));drawEdge(p.detach());}
  std::vector<std::uint8_t>band(256*256);require(edge->readPixels(SkImageInfo::MakeA8(256,256),band.data(),256,0,0),"independentAAread");
  auto a=read(*actual),b=read(*reference);std::size_t bad=0,changed=0;for(std::size_t i=0;i<a.size();++i){const bool different=std::abs(int(a[i])-int(b[i]))>2;changed+=different;bad+=different&&!band[i/4];}std::fprintf(stderr,"mixedvariant%u changed%zu interior%zu\n",variant,changed,bad);require(bad==0,"sharedBodyMixedAlphaMaskAffinePixels");cases.push_back({{"variant",variant},{"nonAAChangedChannels",bad}});
 }
 // Two adjacent68pixel subblocks including2pixelgutter, with thin paths
 // immediately outside one interior. Compare to the identical global sample
 // grid. Gutter sources must influence edge samples, not be AABB-skipped.
 for(float x:{63.25F,64.25F,127.75F,128.25F}){
  auto full=gpu(132,68);auto draw=[&](SkCanvas&d){d.clear(SK_ColorWHITE);SkPaint p;p.setAntiAlias(true);p.setColor(0xff102030);p.setStrokeWidth(.75F);p.setStyle(SkPaint::kStroke_Style);d.drawLine(x,3,x,65,p);p.setStyle(SkPaint::kFill_Style);d.drawRect(SkRect::MakeXYWH(x+.3F,18,.7F,28),p);};full->getCanvas()->translate(2,2);draw(*full->getCanvas());auto combined=gpu(128,64);
  for(int block=0;block<2;++block){auto tile=gpu(68,68);tile->getCanvas()->translate(float(2-block*64),2);draw(*tile->getCanvas());combined->getCanvas()->save();combined->getCanvas()->clipIRect(SkIRect::MakeXYWH(block*64,0,64,64));combined->getCanvas()->drawImage(tile->makeImageSnapshot(),float(block*64-2),-2);combined->getCanvas()->restore();}
  auto whole=read(*full),actual=read(*combined);std::size_t bad=0;for(int y=0;y<64;++y)for(int xx=0;xx<128;++xx)for(int k=0;k<4;++k)bad+=std::abs(int(actual[(y*128+xx)*4+k])-int(whole[((y+2)*132+xx+2)*4+k]))>2;require(bad==0,"64GutterThinLineShapeSeam");cases.push_back({{"gutterX",x},{"nonAAChangedChannels",bad}});
 }

 // Actual production navigation: Runtime-stamped query -> fixedregion job ->
 // allsources publication -> partition/compose -> provider readback. Scale is
 // exactlythefrozenparentdensity so this oracle checks geometry/seams without
 // introducing a new blur/AA tolerance.
 for(unsigned mixed=0;mixed<2;++mixed){
  NativeMetalTarget target(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"regionTarget");
  runtime::MemoryResourceProvider resources;const auto imageId=ObjectId::fromUint64(98900);resources.publish(imageId,{0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0x00,0x05,0x00,0x01,0xff,0x89,0x99,0x3d,0x1d,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82});ImageResourceResolver images(resources);SkiaTextResources text;
  std::vector<ReferenceTraversalEntry> entries;auto add=[&](semantic::ObjectRecord object){
   auto projected=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});require(projected.records.size()==1,"regionProjection");ReferenceTraversalEntry e;e.record=projected.records.front();e.contributesPixels=true;
   if(const auto*c=std::get_if<semantic::ShapeContent>(&object.content))e.command=ShapeReferenceCommand{*c};
   else if(const auto*c=std::get_if<semantic::CompactStrokeContent>(&object.content))e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
   else if(const auto*c=std::get_if<semantic::StickyContent>(&object.content))e.command=StickyReferenceCommand{*c};
   else if(const auto*c=std::get_if<semantic::ImageContent>(&object.content))e.command=ImageReferenceCommand{*c};
   entries.push_back(std::move(e));};
  semantic::ObjectRecord shape;shape.id=ObjectId::fromUint64(10);shape.kind=semantic::ObjectKind::kShape;shape.kind_version=1;shape.content=semantic::ShapeContent{1,2.8,100};shape.transform.tx=1023;shape.transform.ty=70;shape.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{.1F,.3F,.8F,.7F}}}}};add(shape);
  if(mixed){semantic::ObjectRecord image;image.id=ObjectId::fromUint64(19);image.kind=semantic::ObjectKind::kImage;image.kind_version=1;semantic::ImageContent content;content.resource_id.value=imageId;content.intrinsic_width=content.intrinsic_height=1;content.width=110;content.height=80;image.content=content;image.transform.tx=1010;image.transform.ty=50;add(image);}
  auto fine=c;fine.stroke.config.size=3;fine.stroke.config.opacity=.55;fine.stroke.knots={{1023.5,60,40000},{1023.5,190,40000}};
  semantic::ObjectRecord ink;ink.id=ObjectId::fromUint64(11);ink.kind=semantic::ObjectKind::kVectorStroke;ink.kind_version=3;ink.content=fine;add(ink);
  if(mixed){
   auto second=ink;second.id=ObjectId::fromUint64(12);second.transform.tx=8;semantic::FilledPathMask mask;mask.path.commands={semantic::MoveTo{{1010,100}},semantic::LineTo{{1040,100}},semantic::LineTo{{1040,130}},semantic::LineTo{{1010,130}},semantic::ClosePath{}};second.erase_masks={{{},mask}};add(second);
   semantic::ObjectRecord sticky;sticky.id=ObjectId::fromUint64(13);sticky.kind=semantic::ObjectKind::kSticky;sticky.kind_version=1;sticky.content=semantic::StickyContent{70,55};sticky.transform.tx=1010;sticky.transform.ty=150;add(sticky);auto layout=std::make_shared<text::TextLayoutSnapshot>();layout->localBounds={0,0,70,55};layout->decorations={{{5,10,60,13},{0,0,0,1}}};std::get<StickyReferenceCommand>(entries.back().command).labelLayout=layout;
   semantic::ObjectRecord image;image.id=ObjectId::fromUint64(20);image.kind=semantic::ObjectKind::kImage;image.kind_version=1;semantic::ImageContent content;content.resource_id.value=imageId;content.intrinsic_width=content.intrinsic_height=1;content.width=75;content.height=30;image.content=content;image.transform.tx=1040;image.transform.ty=180;add(image);
  }
  FrameState frame{ViewId{1},CameraState{{1024,512},.125F,0,CameraGeneration{1}},{0,0,2048,1024},{256,128,256,128,1,1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{1},{0,0,256,128}};
  SkiaNavigationTiles navigation;NavigationSourceQuery query=[&](const foundation::WorldRect&r,std::vector<ReferenceTraversalEntry>&out,NavigationSourceDiagnostics&d){d.sceneStamped=true;d.semanticGeneration=1;d.sceneRevision=1;for(const auto&e:entries){const auto&b=e.record.worldBounds;if(b.left<r.right&&b.right>r.left&&b.top<r.bottom&&b.bottom>r.top)out.push_back(e);}if(out.empty())d.sceneEmptyProof=NavigationSceneEmptyProof{r,1,1,1,1,1};return true;};
  RetainedNavigationSource retained;retained.bounds={0,0,1,1};retained.density=.125;retained.draw=[](SkCanvas&c,const foundation::WorldRect&){c.drawColor(SK_ColorWHITE,SkBlendMode::kSrc);};
  auto shared=std::make_shared<SkiaInkLayers>();auto geometry=std::make_shared<PreparedGeometryCache>();unsigned calls=0;
  // No parentempty callback: every source/gutter is exercised in actualregion.
  do{auto result=navigation.render(target,{.125F,0,0},frame,query,1,options,budget,false,&retained,&text,&images,{}, {},{},geometry,0xffffffffU,{},true,shared);require(result.code==BackendSubmissionCode::kAccepted,"regionRender");completion->context->flushAndSubmit(GrSyncCpu::kYes);completion->context->checkAsyncWorkCompletion();require(++calls<4096,"regionNeverCompletes");}while(!navigation.work.coverageComplete);
  require(navigation.work.regionPublications>0&&ContinuousZoomProbe::completeRegion(navigation),"productionRegionNotPublished");
  auto expected=gpu(256,128);auto low=gpu(128,64);ReferenceDrawList draw{frame};draw.viewportClip={0,0,256,128};draw.worldToView={.0625,0,0,.0625,0,0};draw.entries=entries;FramePlan plan{frame,std::move(draw),{},false,true};require(drawReferencePlanToSkCanvas(*low->getCanvas(),plan,&images,&text,true,nullptr,nullptr,nullptr).code==BackendSubmissionCode::kAccepted,"regionIndependentDraw");expected->getCanvas()->clear(SK_ColorWHITE);expected->getCanvas()->drawImageRect(low->makeImageSnapshot(),SkRect::MakeWH(256,128),SkSamplingOptions(SkFilterMode::kLinear));
  std::vector<std::uint8_t> actual(256*128*4);require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted,"regionRead");auto info=SkImageInfo::Make(256,128,kRGBA_8888_SkColorType,kPremul_SkAlphaType);std::vector<std::uint8_t> oracle(actual.size());require(expected->readPixels(info,oracle.data(),info.minRowBytes(),0,0),"regionOracleRead");auto edge=SkSurfaces::Raster(SkImageInfo::MakeA8(256,128));edge->getCanvas()->clear(SK_ColorTRANSPARENT);SkPaint boundary;boundary.setColor(SK_ColorWHITE);boundary.setStyle(SkPaint::kStroke_Style);boundary.setStrokeWidth(2);boundary.setAntiAlias(true);
  for(const auto&e:entries){auto m=SkMatrix::Scale(.125F,.125F);const auto&t=e.record.transform;m.preConcat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));auto drawEdge=[&](const SkPath&p){edge->getCanvas()->drawPath(p.makeTransform(m),boundary);};
   if(const auto*ink=std::get_if<CompactStrokeReferenceCommand>(&e.command)){auto contour=ink::compileCompactContour(ink->content.get().stroke.config,ink->content.get().stroke.knots);for(std::size_t i=0;i+1<contour.patches.size();++i)drawEdge(compactContourRibbon(std::span(contour.patches).subspan(i,2)));for(const auto&c:contour.caps){SkPathBuilder p;p.addCircle(c.x,c.y,c.radius);drawEdge(p.detach());
    // Same independent two-source-device-pixel boundary propagated through
    // the fixed linear coarse projection, not a candidate-difference mask.
    auto center=m.mapPoint(SkPoint::Make(float(c.x),float(c.y)));SkPaint capBoundary=boundary;capBoundary.setStrokeWidth(4);edge->getCanvas()->drawCircle(center.x(),center.y(),float(c.radius*.125),capBoundary);
   }}
   else {SkPathBuilder p;const auto b=e.record.geometryBounds;p.addRect(SkRect::MakeLTRB(b.left-t.tx,b.top-t.ty,b.right-t.tx,b.bottom-t.ty));drawEdge(p.detach());}
  }
  std::vector<std::uint8_t> band(256*128);require(edge->readPixels(SkImageInfo::MakeA8(256,128),band.data(),256,0,0),"regionIndependentEdge");std::size_t bad=0,changed=0;unsigned maximum=0;for(std::size_t i=0;i<actual.size();++i){auto delta=unsigned(std::abs(int(actual[i])-int(oracle[i])));changed+=delta>2;if(delta>2&&!band[i/4])std::fprintf(stderr,"nonAAx%zu y%zu channel%zu a%u b%u\n",(i/4)%256,(i/4)/256,i%4,unsigned(actual[i]),unsigned(oracle[i]));bad+=(delta>2&&!band[i/4]);maximum=std::max(maximum,delta);}std::fprintf(stderr,"productionregion mixed%u changed%zu interior%zu max%u pubs%llu calls%u\n",mixed,changed,bad,maximum,(unsigned long long)navigation.work.regionPublications,calls);require(bad==0,"productionRegionGutterOrOrderPixels");cases.push_back({{"productionRegionMixed",mixed},{"publications",navigation.work.regionPublications},{"calls",calls},{"changedBeyond2",bad}});
 }
 std::ofstream(argv[1])<<nlohmann::json{{"status","PASS"},{"cases",cases},{"actualGPU",true},{"bodyImmutableMasksIndependent",true},{"orderedInterleave",true}}.dump(2)<<'\n';return 0;
}
