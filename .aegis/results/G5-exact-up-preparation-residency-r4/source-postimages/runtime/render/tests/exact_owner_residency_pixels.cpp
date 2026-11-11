#define main ownerPixelFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/skia_ink_layers.hpp"
#include "../src/skia_mask_history.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
int main(){try{
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 {auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();
  GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"owner native GPU");
  for(unsigned variant=0;variant<3;++variant){
   auto budget=std::make_shared<resources::ResourceBudgetManager>();auto sources=std::make_shared<SourceRetentionPool>();auto geometry=std::make_shared<PreparedGeometryCache>();geometry->setRetentionPool(sources);
   auto histories=std::make_shared<EraseMaskPathCache>();histories->setRetentionPool(sources);RenderCacheOptions options;
   auto e=source();e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.kindVersion=3;
   e.record.content.update([&](auto&v){auto&s=std::get<semantic::CompactStrokeContent>(v).stroke;s.config.opacity=variant?.55:1;s.knots={{20,64,32768},{236,64,32768}};});
   e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
   semantic::FilledPathMask strip;strip.path.commands={semantic::MoveTo{{80,0}},semantic::LineTo{{96,0}},semantic::LineTo{{96,128}},semantic::LineTo{{80,128}},semantic::ClosePath{}};
   auto rightStrip=strip;rightStrip.path.commands={semantic::MoveTo{{180,0}},semantic::LineTo{{196,0}},semantic::LineTo{{196,128}},semantic::LineTo{{180,128}},semantic::ClosePath{}};
   e.record.eraseMasks={{ObjectId::fromUint64(900),strip},{ObjectId::fromUint64(901),rightStrip}};
   auto surface=SkSurfaces::RenderTarget(completion->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(256,128));require(bool(surface),"owner pixels target");
   CompactCoverageTargets targets;targets.budget=budget;SkiaInkLayers layers;RenderCacheWork work;
   auto body=geometry->get(e,options,budget,true);require(body&&body->ready(),"owner pixels body");
   const auto info=SkImageInfo::Make(256,128,kRGBA_8888_SkColorType,kPremul_SkAlphaType);
   auto draw=[&](bool narrow){surface->getCanvas()->clear(variant==2?SK_ColorBLUE:SK_ColorWHITE);surface->getCanvas()->save();if(narrow)surface->getCanvas()->clipRect(SkRect::MakeXYWH(0,0,128,128));
    bool painted=false,pending=false;require(layers.draw(*surface->getCanvas(),*surface,e,SkMatrix::I(),SkIRect::MakeWH(256,128),nullptr,options,budget,sources,targets,work,painted,pending,geometry,body,histories)&&!pending,"owner pixels draw");surface->getCanvas()->restore();completion->context->flushAndSubmit(GrSyncCpu::kYes);
    std::vector<std::uint8_t> bytes(256*128*4);require(surface->readPixels(info,bytes.data(),info.minRowBytes(),0,0),"owner pixels read");return bytes;};
   const auto original=draw(false);auto external=histories->history(e.record.eraseMasks,budget,8*1024*1024);require(external&&external->primitives.size()==2,"external plan fixture");
   auto primitive=external->primitives.front();const auto externalBytes=external->bytes;const auto beforeBody=work.bodyTileBuilds;
   require(layers.retireIdleHistoryPreparation()==1,"owner pixel retirement");
   require(external->primitives.size()==2&&external->primitives.front()==primitive&&external->bytes==externalBytes,"externally held plan mutated");
   require(draw(false)==original&&work.bodyTileBuilds==beforeBody,"same history retirement pixels/body changed");
   // The right tile is offscreen during the first rebuild. It must lose mask
   // qualification too, otherwise null-prefix would preserve stale clears.
   require(layers.retireIdleHistoryPreparation()==1,"second owner retirement");e.record.eraseMasks.clear();(void)draw(true);
   const auto revealed=draw(false);require(revealed!=original,"removed history retained stale masked pixels");require(work.bodyTileBuilds==beforeBody,"offscreen retirement discarded body");
   e.record.eraseMasks={{ObjectId::fromUint64(900),strip},{ObjectId::fromUint64(901),rightStrip}};require(draw(false)==original,"history redo after owner retirement");
   layers.clear();histories->clear();body.reset();geometry->clear();targets.clear();external.reset();
   // A foreign pin remains valid/charged even after both owners clear.
   require(primitive->path.isFinite(),"foreign prepared primitive invalidated");
   primitive.reset();require(budget->used()==0,"owner pixel last foreign/GPU lease leak");
   completion->context->flushAndSubmit(GrSyncCpu::kYes);
   std::cout<<nlohmann::json{{"variant",variant},{"external_plan_immutable",true},{"all_tiles_requalified",true},{"body_builds_retained",true},{"undo_redo_pixels",true}}.dump()<<'\n';
  }
 }message(pool,"drain");
#endif
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
