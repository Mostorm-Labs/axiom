#include "canvas/ink/compact_contour.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/selection_region_geometry.hpp"
#include "../src/skia_prepared_geometry.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "../src/skia_geometry_internal.hpp"
#include "include/core/SkPathUtils.h"
#include <fstream>
#include <iterator>
#include "include/core/SkPathBuilder.h"
#include "include/core/SkMatrix.h"
#include <limits>
#include <deque>
#include "../src/compact_contour_skia_path.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
using namespace canvas;
namespace {
void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
foundation::InkBrushConfig config(){auto c=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.startCap=c.endCap=false;return c;}
std::vector<foundation::InkKnot> hairpin(){return {{350,40,32768},{200,80,32768},{70,150,32768},{220,160,32768},{360,150,32768}};}
std::vector<SkPath> paths(const ink::CompactContour& c){std::vector<SkPath> result;for(std::size_t i=0;i+1<c.patches.size();++i)result.push_back(render::internal::compactContourRibbon(std::span(c.patches).subspan(i,2)));return result;}
bool body(const std::vector<SkPath>& p,const ink::CompactContour& c,double x,double y,const foundation::InkBrushConfig* config=nullptr){
 for(const auto& v:p)if(v.contains(float(x),float(y)))return true;
 for(auto cap:c.caps){auto dx=x-cap.x,dy=y-cap.y;
  if(config&&config->family==foundation::InkFamily::kMarker){const auto a=config->markerAngle;const auto u=dx*std::cos(a)+dy*std::sin(a),v=(-dx*std::sin(a)+dy*std::cos(a))/config->markerFlatness;dx=u;dy=v;}
  if(std::hypot(dx,dy)<=cap.radius)return true;
 }return false;
}
struct Shape {bool node=true,neck=true,support=true;};
// Expected witnesses are derived from the captured sparse arms and fixed brush
// radius, not by another call to a production contour/helper. A deletion-only
// implementation loses the node; a giant circle violates the support envelope.
Shape shape(const ink::CompactContour& c,const std::vector<foundation::InkKnot>& k,double r){
 const auto p=paths(c);const auto corner=k[2];Shape result;
 result.node=body(p,c,corner.x,corner.y)&&body(p,c,corner.x-r*.65,corner.y);
 for(auto index:{1U,3U}){const auto dx=k[index].x-corner.x,dy=k[index].y-corner.y,l=std::hypot(dx,dy);
  for(double d:{10.,14.,18.,22.,26.,30.})result.neck=result.neck&&body(p,c,corner.x+dx*d/l,corner.y+dy*d/l);
 }
 // Round support is bounded at the exposed extremum. All raw input lies to
 // the right of x=corner.x; no valid round join can protrude left by >r.
 for(double y=corner.y-32;y<=corner.y+32;y+=.5)result.support=result.support&&!body(p,c,corner.x-r-1,y);
 return result;
}
void image(const foundation::InkBrushConfig& c,const ink::CompactContour& contour,const std::filesystem::path& file){
 auto s=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(820,440));s->getCanvas()->clear(SK_ColorWHITE);s->getCanvas()->scale(2,2);s->getCanvas()->translate(5,10);
 render::internal::drawCompactContourCoverageToSkCanvas(*s->getCanvas(),c,contour);
 // White coverage on white is invisible; use the exact same geometry in black.
 SkPaint paint;paint.setAntiAlias(true);paint.setColor(SkColorSetRGB(13,26,51));for(auto p:paths(contour))s->getCanvas()->drawPath(p,paint);
 for(auto cap:contour.caps)s->getCanvas()->drawCircle(float(cap.x),float(cap.y),float(cap.radius),paint);
 SkFILEWStream stream(file.string().c_str());need(stream.isValid(),"image stream");SkPixmap pixels;need(s->peekPixels(&pixels)&&SkPngEncoder::Encode(&stream,pixels,{}),"PNG encode");
}

unsigned connectedComponents(const foundation::InkBrushConfig& c,const ink::CompactContour& contour){
 constexpr unsigned width=1680,height=960;auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));surface->getCanvas()->clear(SK_ColorBLACK);surface->getCanvas()->scale(4,4);
 render::internal::drawCompactContourCoverageToSkCanvas(*surface->getCanvas(),c,contour);std::vector<std::uint8_t> rgba(width*height*4);need(surface->readPixels(surface->imageInfo(),rgba.data(),width*4,0,0),"connectivity read");
 std::vector<bool> seen(width*height);unsigned components=0;std::vector<unsigned> queue;
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto start=y*width+x;if(seen[start]||rgba[start*4]<128)continue;++components;queue.clear();queue.push_back(start);seen[start]=true;
  for(std::size_t at=0;at<queue.size();++at){const auto p=queue[at],px=p%width,py=p/width;
   for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const auto nx=int(px)+dx,ny=int(py)+dy;if(nx<0||ny<0||nx>=int(width)||ny>=int(height))continue;const auto next=unsigned(ny)*width+unsigned(nx);if(!seen[next]&&rgba[next*4]>=128){seen[next]=true;queue.push_back(next);}}}
 }
 return components;
}
void independent(const std::filesystem::path& dir){
 auto c=config();auto k=hairpin();const auto contour=ink::compileCompactContour(c,k);const auto r=ink::compactRadius(c,32768);const auto v=shape(contour,k,r);
 if(!dir.empty()){std::filesystem::create_directories(dir);image(c,contour,dir/"sparse-hairpin.png");}
 std::cout<<"shape node="<<v.node<<" neck="<<v.neck<<" support="<<v.support<<" patches="<<contour.patches.size()<<" caps="<<contour.caps.size()<<std::endl;
 need(v.node,"corner presence missing (deletion-only is invalid)");need(v.neck,"corner is disconnected from incoming/outgoing neck");need(v.support,"join exceeds independent support envelope");need(connectedComponents(c,contour)==1,"stroke contains a detached raster component");
 // Counterexample uses the archived old flawed output, not deletion of a
 // corrected proper ribbon. Its exact knots/control points are a fixed probe.
 ink::CompactContour deleted;
 deleted.patches={
  {{347.938668,32.2700037},{347.938668,32.2700037},{272.915205,52.2793007},{352.061332,47.7299963},{352.061332,47.7299963},{277.084795,67.7206993}},
  {{272.915205,52.2793007},{197.891743,72.2885978},{132.049444,107.622361},{277.084795,67.7206993},{202.108257,87.7114022},{137.950556,122.377639}},
  {{132.049444,107.622361},{66.2071441,142.956125},{142.842722,155.469251},{137.950556,122.377639},{73.7928559,157.043875},{147.157278,154.530749}},
  {{142.842722,155.469251},{219.478299,167.982377},{290.024142,162.981084},{147.157278,154.530749},{220.521701,152.017623},{289.975858,147.018916}},
  {{290.024142,162.981084},{360.569985,157.97979},{360.569985,157.97979},{289.975858,147.018916},{359.430015,142.02021},{359.430015,142.02021}}
 }; // Exact archived old ribbons, with only the orphan disk deleted.
 need(!shape(deleted,k,r).node,"shape oracle failed to reject deletion-only corner");
 auto oldIsolated=deleted;oldIsolated.caps.push_back({70,150,r});need(!shape(oldIsolated,k,r).neck&&connectedComponents(c,oldIsolated)>1,"shape oracle failed to detect archived old island");
 auto giant=contour;giant.caps.push_back({70,150,80});need(!shape(giant,k,r).support,"shape oracle failed to reject oversized join");
}
std::vector<std::uint8_t> coverage(const foundation::InkBrushConfig& c,const ink::CompactContour& contour){
 auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(420,240));surface->getCanvas()->clear(SK_ColorBLACK);
 render::internal::drawCompactContourCoverageToSkCanvas(*surface->getCanvas(),c,contour);std::vector<std::uint8_t> pixels(420*240*4);
 need(surface->readPixels(surface->imageInfo(),pixels.data(),420*4,0,0),"coverage read");return pixels;
}
void streamed(){
 using namespace render::internal;
 for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker})for(bool taper:{false,true})for(unsigned lead:{28U,29U,30U,252U,253U,254U}){
  auto c=config();c.family=family;c.markerAngle=.7;c.markerFlatness=.25;if(taper){c.startTaper=8;c.endTaper=24;}
  auto k=hairpin();for(unsigned i=0;i<k.size();++i)k[i].pressure=std::uint16_t(10000+i*9000);
  // Long straight prefix/trail puts the corner on every relevant block edge.
  std::vector<foundation::InkKnot> longKnots;for(unsigned i=0;i<lead;++i)longKnots.push_back({350.+(lead-i)*.5,40,10000});
  longKnots.insert(longKnots.end(),k.begin(),k.end());for(unsigned i=0;i<300;++i)longKnots.push_back({360.+i*.5,150,46000});k=std::move(longKnots);
  const auto full=ink::compileCompactContour(c,k);const auto expected=coverage(c,full);const auto total=ink::compactLength(k);
  for(unsigned block:{1U,2U,31U,32U,255U,256U}){
   auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(420,240));surface->getCanvas()->clear(SK_ColorBLACK);ink::CompactContourCompiler compiler(c);
   for(std::size_t at=0;at<k.size();at+=block){auto checkpoint=compiler;ink::CompactContour emitted,cloned;const auto n=std::min(std::size_t(block),k.size()-at);
    compiler.append(std::span(k).subspan(at,n),total,emitted);checkpoint.append(std::span(k).subspan(at,n),total,cloned);
    if(at+n==k.size()){compiler.finish(total,emitted);checkpoint.finish(total,cloned);}
    need(emitted.patches==cloned.patches&&emitted.caps.size()==cloned.caps.size(),"checkpoint continuation geometry differs");
    render::internal::drawCompactContourCoverageToSkCanvas(*surface->getCanvas(),c,emitted);
   }
   std::vector<std::uint8_t> actual(expected.size());need(surface->readPixels(surface->imageInfo(),actual.data(),420*4,0,0),"stream read");
   for(std::size_t i=0;i<actual.size();++i)need(std::abs(int(actual[i])-int(expected[i]))<=2,"chunk seam/corner pixel mismatch");
  }
 }
 std::cout<<"full/chunk1,2,31,32,255,256/checkpoint parity\n";
}
void variants(){
 auto c=config();
 const std::vector<std::vector<foundation::InkKnot>> cases{
  hairpin(),{{70,40,32768},{70,150,32768},{70,50,32768}},
  {{70,40,32768},{70,150,32768},{71,50,32768}},
  {{70,40,32768},{70,150,32768},{180,149,32768}},
  {{70,40,32768},{70,150,32768},{180,151,32768}},
  {{200,40,32768},{70,150,32768},{200,155,32768},{75,160,32768},{210,165,32768}},
  {{200,40,0},{70,150,65535},{70,150,65535},{220,160,0}}};
 for(auto base:cases)for(bool mirror:{false,true})for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker})for(double flatness:{.25,.05}){
  c.family=family;c.markerAngle=.7;c.markerFlatness=flatness;if(mirror)for(auto& p:base)p.x=420-p.x;
  const auto contour=ink::compileCompactContour(c,base);need(contour.patches.size()<=ink::CompactContourCompiler::maxPatches(base.size()),"patch bound");
  for(auto p:paths(contour))need(p.isFinite(),"nonfinite reversal path");
  // Every actual corner disk/ellipse is contained in the fixed support at
  // that input center; neither round nor marker may use an enlarged join.
  for(auto cap:contour.caps){const auto it=std::find_if(base.begin(),base.end(),[&](auto k){return k.x==cap.x&&k.y==cap.y;});need(it!=base.end(),"corner relocated from captured center");
   need(std::abs(cap.radius-ink::compactRadius(c,it->pressure))<1e-9,"corner pressure radius changed");
   const auto p=paths(contour);need(body(p,contour,cap.x,cap.y,&c),"corner coverage lost");
   // In the nib-normalized domain the cap remains a unit disk, even at .05.
   const auto a=c.markerAngle,r=cap.radius;const auto ux=family==foundation::InkFamily::kMarker?std::cos(a):1.,uy=family==foundation::InkFamily::kMarker?std::sin(a):0.;
   need(body({},contour,cap.x+ux*r*.9,cap.y+uy*r*.9,&c),"nib support inside missing");
  }
 }
 // Exact independent neck probes on mirrored sparse round input.
 for(bool mirrored:{false,true}){auto k=hairpin();if(mirrored)for(auto& p:k)p.x=420-p.x;const auto contour=ink::compileCompactContour(config(),k);const auto p=paths(contour);auto point=k[2];for(unsigned arm:{1U,3U}){const auto dx=k[arm].x-point.x,dy=k[arm].y-point.y,l=std::hypot(dx,dy);for(double d:{10.,14.,18.,22.,26.,30.})need(body(p,contour,point.x+dx*d/l,point.y+dy*d/l),"mirrored independent neck gap");}}

 // Independent marker neck and convex support witnesses in world space.
 // The nib's minimum semiaxis is .05r; sample the actual filled geometry at
 // fractional-world resolution so thin markers cannot pass as disconnected.
 for(double flatness:{.25,.05}){
  auto marker=config();marker.family=foundation::InkFamily::kMarker;marker.markerAngle=.7;marker.markerFlatness=flatness;
  const auto k=hairpin();const auto contour=ink::compileCompactContour(marker,k);need(connectedComponents(marker,contour)==1,"thin marker has disconnected raster island");const auto ribbon=paths(contour);const auto corner=k[2];const auto r=ink::compactRadius(marker,32768);
  for(unsigned arm:{1U,3U}){auto dx=k[arm].x-corner.x,dy=k[arm].y-corner.y;auto length=std::hypot(dx,dy);
   for(double distance=.1;distance<36;distance+=.1)need(body(ribbon,contour,corner.x+dx*distance/length,corner.y+dy*distance/length,&marker),"marker independent incoming/outgoing neck gap");}
  const auto ca=std::cos(marker.markerAngle),sa=std::sin(marker.markerAngle);
  const auto supportX=r*std::hypot(ca,flatness*sa);for(double y=corner.y-20;y<=corner.y+20;y+=.1)
   need(!body(ribbon,contour,corner.x-supportX-.25,y,&marker),"marker join outside ellipse support");
 }
 auto sparse=hairpin();std::vector<foundation::InkKnot> dense;
 for(std::size_t i=1;i<sparse.size();++i){const auto a=sparse[i-1],b=sparse[i];if(dense.empty())dense.push_back(a);
  for(unsigned n=1;n<=32;++n)dense.push_back({std::lerp(a.x,b.x,double(n)/32),std::lerp(a.y,b.y,double(n)/32),32768});}
 auto denseContour=ink::compileCompactContour(config(),dense);need(connectedComponents(config(),denseContour)==1,"densified corner has detached component");
 std::cout<<"U/90/180/adjacent/mirror/variablepressure/marker finite support\n";
}
render::ReferenceTraversalEntry entry(const foundation::InkBrushConfig& c,const std::vector<foundation::InkKnot>& k){
 semantic::CompactStrokeContent content;content.stroke.config=c;content.stroke.knots=k;content.stroke.seed=42;
 render::ReferenceTraversalEntry e;e.record.objectId=ObjectId::fromUint64(91);e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.content=content;
 e.command=render::CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};return e;
}
void prepared(){
 using namespace render;using namespace render::internal;
 auto c=config();std::vector<foundation::InkKnot> k;for(unsigned i=0;i<8192;++i)k.push_back({i%2?90.:290.,30.+i*.015,32768});
 auto e=entry(c,k);auto budget=std::make_shared<resources::ResourceBudgetManager>();PreparedGeometryCache cache;RenderCacheOptions limits;
 // A small body exercises stored prepared paths as well as the large
 // checkpoint realization below. Actual admissions share one cache owner.
 {auto small=entry(c,hairpin());auto full=cache.request(small,limits,budget);need(bool(full),"small full-path admission");
  while(!full->ready()){PreparationAllowance allowance;cache.advance(full,allowance);need(!full->failed(),"small full-path preparation");}
  auto target=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(420,240));target->getCanvas()->clear(SK_ColorBLACK);full->drawContour(*target->getCanvas());
  std::vector<std::uint8_t> actual(420*240*4);need(target->readPixels(target->imageInfo(),actual.data(),420*4,0,0),"small prepared read");need(actual==coverage(c,ink::compileCompactContour(c,hairpin())),"small stored paths differ from full contour");cache.clear();full.reset();need(budget->used()==0,"small full-path lease leak");}
 limits.maxEntryBytes=2*1024*1024;auto body=cache.request(e,limits,budget);need(bool(body)&&!body->ready(),"bounded cold admission failed");
 RenderPreparationBudget one;one.maxCoverageSteps=1;unsigned steps=0;
 while(!body->ready()&&!body->failed()){const auto before=cache.statistics();PreparationAllowance allowance(one);cache.advance(body,allowance);const auto after=cache.statistics();
  need(after.compileInputs-before.compileInputs<=32&&after.pathPrimitives-before.pathPrimitives<=32,"corner expansion exceeded peradvance quantum");need(++steps<4096,"corner build failed to progress");}
 need(body->ready()&&!body->failed(),"corner replay underestimated storage");
 auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(420,240));actual->getCanvas()->clear(SK_ColorBLACK);body->drawContour(*actual->getCanvas());const auto expected=coverage(c,ink::compileCompactContour(c,k));std::vector<std::uint8_t> pixels(expected.size());need(actual->readPixels(actual->imageInfo(),pixels.data(),420*4,0,0),"prepared read");need(pixels==expected,"prepared checkpoint sharpturn pixels differ");
 const auto used=budget->used();need(used<=limits.maxEntryBytes,"prepared entry escaped cap");cache.clear();body.reset();need(budget->used()==0,"corner leases leaked");
 auto denied=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});need(!cache.request(e,limits,denied)&&denied->used()==0,"corner allocation escaped hard limit");
 need(ink::CompactContourCompiler::maxPatches(std::numeric_limits<std::size_t>::max())==std::numeric_limits<std::size_t>::max(),"bound overflow");
 std::cout<<"8192 everyknotturn checkpoint/32knot32path/cap/lease "<<used<<"bytes\n";
}
semantic::EraseMaskGeometry box(double x,double y,double w,double h){semantic::FilledPathMask m;m.path.commands={semantic::MoveTo{{x,y}},semantic::LineTo{{x+w,y}},semantic::LineTo{{x+w,y+h}},semantic::LineTo{{x,y+h}},semantic::ClosePath{}};return m;}

void preview(){
 for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker}){
  auto package=ink::BrushPackageCatalog{}.loadDefault(family==foundation::InkFamily::kRound?"round-handdrawn":"marker-compact-v1",4).package;
  package.compactConfig->markerAngle=.7;package.compactConfig->markerFlatness=.25;
  ink::BrushSession session(71,ink::resolveBrushState(package,42)),batched(72,ink::resolveBrushState(package,42));need(session.begin()&&batched.begin(),"preview begin");
  render::TransparentOverlaySkiaSurfaceProvider provider;need(provider.resize(420,240).code==render::BackendSubmissionCode::kAccepted,"preview surface");render::SkiaRenderer renderer;render::PreviewSurfaceController controller(renderer,provider);need(controller.begin(1,71,1,provider.generation()),"preview controller");render::PreviewStyleOverride style;style.preferCapturedPaint=true;
  std::vector<ink::BrushSample> raw;for(unsigned i=0;i<160;++i){auto k=hairpin()[i%5];raw.push_back({k.x,k.y,0,false,i+1,std::uint64_t(i+1)*8'000'000,false,i==159});}
  ink::BrushPreviewDelta delta,batchDelta;
  for(std::size_t i=0;i<raw.size();++i){auto prediction=raw[i];prediction.sequence+=10000;prediction.timestampNs+=10'000'000;prediction.x+=40;
   const std::span<const ink::BrushSample> predicted=i+1==raw.size()?std::span<const ink::BrushSample>{}:std::span<const ink::BrushSample>{&prediction,1};
   need(session.append({&raw[i],1},predicted,delta)==ink::BrushSessionError::kNone&&controller.update(delta),"preview append");
   need(controller.renderIfDirty(style),"preview draw");}
  need(batched.append(raw,{},batchDelta)==ink::BrushSessionError::kNone,"batch append");ink::BrushCommitIntent sealed,batch;need(session.seal(sealed)==ink::BrushSessionError::kNone&&batched.seal(batch)==ink::BrushSessionError::kNone,"preview seal");need(sealed.knots==batch.knots,"predicted or batching changed persisted knots");
  std::vector<const ink::BrushPreviewDelta::Chunk*> chunks;for(auto v=delta.compactChunks.get();v;v=v->previous.get())chunks.push_back(v);
  ink::CompactContour collected;for(auto i=chunks.rbegin();i!=chunks.rend();++i){const auto& v=(*i)->contour;
   for(std::size_t p=0;p<v.patches.size();++p)if(collected.patches.empty()||p||collected.patches.back()!=v.patches[p])collected.patches.push_back(v.patches[p]);
   collected.caps.insert(collected.caps.end(),v.caps.begin(),v.caps.end());}
  const auto full=ink::compileCompactContour(*package.compactConfig,sealed.knots);
  need(collected.patches==full.patches&&collected.caps.size()==full.caps.size(),"prefix/tail compiler geometry differs from sealed full history");
  auto expected=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(420,240));expected->getCanvas()->clear(SK_ColorTRANSPARENT);auto scalar=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(420,240));scalar->getCanvas()->clear(SK_ColorBLACK);
  render::internal::drawCompactContourCoverageToSkCanvas(*scalar->getCanvas(),*package.compactConfig,full);
  render::internal::compositeCompactCoverageToSkCanvas(*expected->getCanvas(),*scalar->makeImageSnapshot(),*package.compactConfig,42);
  std::vector<std::uint8_t> actual(420*240*4),reference(actual.size());need(provider.readbackRgba(actual).code==render::BackendSubmissionCode::kAccepted,"preview read");auto info=SkImageInfo::Make(420,240,kRGBA_8888_SkColorType,kPremul_SkAlphaType);need(expected->readPixels(info,reference.data(),420*4,0,0),"sealed read");
  unsigned differences=0,maximum=0;for(std::size_t i=3;i<actual.size();i+=4){auto v=unsigned(std::abs(int(actual[i])-int(reference[i])));differences+=v>2;maximum=std::max(maximum,v);}
  std::cout<<"previewfamily="<<unsigned(family)<<" differences="<<differences<<" max="<<maximum<<std::endl;need(!differences,"stableprefix/tail/seal corner coverage mismatch");
 }
 std::cout<<"BrushSession stableprefix/prediction replace/final-seal preview parity\n";
}
void exact(){
 auto e=entry(config(),hairpin());const foundation::WorldPoint neck[]{{82,145},{86,145},{86,149},{82,149}};
 need(render::intersectVisibleSelectionGeometry(e.record,neck)==render::SelectionGeometryHit::kHit,"corrected neck not selectable");
 const auto mask=box(81,143,8,8);need(render::intersectEraseGeometry(e.record,mask)==render::SelectionGeometryHit::kHit,"corrected neck not erasable");
 e.record.eraseMasks={{ObjectId::fromUint64(92),mask}};need(render::intersectEraseGeometry(e.record,box(82,144,3,3))==render::SelectionGeometryHit::kMiss,"neck erase historical mask ignored");
 std::cout<<"corrected neck exact selection/erase/mask subtraction\n";
}

void webOracle(const std::filesystem::path& snapshot,const std::filesystem::path& output){
 std::ifstream input(snapshot,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
 auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(decoded.ok(),"actual Web snapshot decode");
 const auto& snapshotValue=*decoded.snapshot;const auto& objects=snapshotValue.objects;const auto projection=projectRuntimeScene(objects,semantic::SemanticGeneration{1});
 semantic::IndexedObjectStore store;semantic::DocumentRuntimeState state=semantic::DocumentRuntimeState::kLoading;semantic::ResourceManifest manifest;semantic::SemanticGenerationState generation{semantic::SemanticGeneration{1}};
 const semantic::SnapshotBootstrapper::CheckpointBinding binding{snapshotValue.document_id,snapshotValue.document_revision,snapshotValue.recovery_frontier,snapshotValue.checkpoint_digest};
 need(semantic::SnapshotBootstrapper::restore(snapshotValue,semantic::SnapshotCapability::kAllWithCompactStrokeV1,state,store,manifest,generation,binding).restored,"oracle semantic restore");
 const semantic::SemanticReadView view(store,generation.current());RuntimeScene scene;need(bool(scene.replace(view)),"oracle scene projection");
 render::FrameState frame{render::ViewId{1},render::CameraState{{450,325},1,0,render::CameraGeneration{1}},
  {0,0,900,650},{900,650,1800,1300,2,1},generation.current(),SceneRevision{1},render::SurfaceGeneration{1},render::MetricsGeneration{1},render::FrameId{1},{0,0,900,650}};
 render::VisibilityResult visible{frame.sceneGeneration,frame.sceneReadToken,frame.worldViewport};for(const auto& obj:projection.records)visible.backToFront.push_back(obj.objectId);visible.visibleRecords=visible.backToFront.size();
 auto reference=render::DirectReferenceSource::build(frame,visible,scene);need(bool(reference),"oracle references");auto plan=render::FramePlanBuilder::build(frame,reference.value(),decoded.snapshot->background,false,true);need(bool(plan),"oracle plan");
 auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1800,1300));need(render::internal::drawReferencePlanToSkCanvas(*surface->getCanvas(),plan.value()).code==render::BackendSubmissionCode::kAccepted,"oracle full direct draw");
 auto edge=SkSurfaces::Raster(SkImageInfo::MakeA8(1800,1300));edge->getCanvas()->clear(SK_ColorTRANSPARENT);SkPaint stroke;stroke.setColor(SK_ColorWHITE);stroke.setAntiAlias(true);stroke.setStyle(SkPaint::kStroke_Style);stroke.setStrokeWidth(2);
 for(const auto& e:reference.value().entries){const auto& content=std::get<render::CompactStrokeReferenceCommand>(e.command).content.get().stroke;const auto contour=ink::compileCompactContour(content.config,content.knots);const auto& t=e.record.transform;
  SkMatrix matrix;matrix.setAll(float(2*t.a),float(2*t.c),float(2*t.tx),float(2*t.b),float(2*t.d),float(2*t.ty),0,0,1);
  for(auto p:paths(contour))edge->getCanvas()->drawPath(p.makeTransform(matrix),stroke);
  for(auto cap:contour.caps){SkPathBuilder builder;builder.addCircle(0,0,float(cap.radius));SkMatrix capMatrix;capMatrix.setTranslate(float(cap.x),float(cap.y));if(content.config.family==foundation::InkFamily::kMarker){capMatrix.preRotate(float(content.config.markerAngle*57.29577951308232));capMatrix.preScale(1,float(content.config.markerFlatness));}edge->getCanvas()->drawPath(builder.detach().makeTransform(capMatrix).makeTransform(matrix),stroke);}
  for(const auto& m:e.record.eraseMasks){if(const auto* f=std::get_if<semantic::FilledPathMask>(&m.geometry))edge->getCanvas()->drawPath(render::internal::semanticPath(f->path).makeTransform(matrix),stroke);
   else if(const auto* sweep=std::get_if<semantic::SweptCircleMask>(&m.geometry))for(const auto& p:sweep->segments){SkPathBuilder builder;builder.moveTo(float(p.p0.position.x),float(p.p0.position.y));builder.cubicTo(float(p.control1.x),float(p.control1.y),float(p.control2.x),float(p.control2.y),float(p.p1.position.x),float(p.p1.position.y));SkPaint mask;mask.setStyle(SkPaint::kStroke_Style);mask.setStrokeWidth(float(2*std::max(p.p0.radius,p.p1.radius)));mask.setStrokeCap(SkPaint::kRound_Cap);edge->getCanvas()->drawPath(skpathutils::FillPathWithPaint(builder.detach(),mask).makeTransform(matrix),stroke);}}
 }
 std::vector<std::uint8_t> rgba(1800*1300*4),edges(1800*1300);auto info=SkImageInfo::Make(1800,1300,kRGBA_8888_SkColorType,kPremul_SkAlphaType);need(surface->readPixels(info,rgba.data(),1800*4,0,0)&&edge->readPixels(edge->imageInfo(),edges.data(),1800,0,0),"oracle readback");
 std::ofstream(output.string()+".rgba",std::ios::binary).write(reinterpret_cast<const char*>(rgba.data()),rgba.size());std::ofstream(output.string()+".edges",std::ios::binary).write(reinterpret_cast<const char*>(edges.data()),edges.size());
 std::cout<<"two-stroke snapshot direct CPU/independently projected 1pixel contour-boundary band, no candidate difference derived band\n";
}

}
int main(int argc,char**argv){try{if(argc==4&&std::string(argv[1])=="--web-oracle"){webOracle(argv[2],argv[3]);return 0;}independent(argc>1?argv[1]:"");variants();streamed();prepared();preview();exact();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
