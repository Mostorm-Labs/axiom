#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/render/image_resource.hpp"
#include <cassert>
#include <limits>
#include <thread>
#include <fstream>
#include "canvas/semantic/codec.hpp"

namespace {
using namespace canvas;
semantic::ObjectRecord rectangle(std::uint64_t id,double x,semantic::ColorValue color) {
  semantic::ObjectRecord object;
  object.id=foundation::ObjectId::fromUint64(id);object.kind_version=1;
  object.content=semantic::ShapeContent{1,100,80};object.transform.tx=x;object.transform.ty=20;
  object.placement.order_key=semantic::OrderKey{{static_cast<std::uint8_t>(id)}};
  object.properties.entries.push_back({0x100U,semantic::FillStyleValue{semantic::SolidFill{color}}});
  return object;
}
struct PublicationProbe {
  runtime::RuntimeFacade* facade = nullptr;
  runtime::ImageExportStatus result = runtime::ImageExportStatus::kExported;
  static void applied(void* context,const semantic::Operation&,const semantic::CanonicalCommitRecord&) noexcept {
    auto& self=*static_cast<PublicationProbe*>(context);
    self.result=self.facade->exportImage({}).status;
  }
};
std::shared_ptr<const render::DecodedImage> decode(const runtime::ImageExportResult& image) {
  runtime::MemoryResourceProvider resources;
  const auto id=foundation::ObjectId::fromUint64(9);resources.publish(id,image.bytes);
  render::ImageResourceResolver decoder(resources);
  const auto decoded=decoder.resolve(semantic::ResourceId{id});
  assert(decoded->state==render::ImageResourceState::kReady);
  return decoded;
}
}
int main(int argc,char** argv) {
  using namespace canvas;
  const auto document=foundation::ObjectId::fromUint64(0xE001);
  runtime::RuntimeComposition owner;
  assert(owner.openDocument(document));
  assert(owner.controlFacade().exportImage({}).status==runtime::ImageExportStatus::kNotReady);
  assert(owner.bindSurface(256,256));
  auto& facade=owner.controlFacade();
  runtime::ImageExportOptions options;options.padding=0;
  const auto empty=facade.exportImage(options);
  assert(empty.status==runtime::ImageExportStatus::kExported&&empty.width==256&&empty.height==256);
  const auto white=decode(empty);assert(white->rgba[0]==255&&white->rgba[3]==255);
  const semantic::Operation insert{semantic::OperationId{foundation::ObjectId::fromUint64(1)},
    semantic::DocumentId{document},1,1,semantic::InsertObjectsOp{{rectangle(1,-100,{1,0,0,1}),rectangle(2,400,{0,0,1,1})}}};
  assert(owner.applyRemoteOperation(insert).disposition==semantic::ApplyDisposition::kApplied);
  const auto before=facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
  runtime::DocumentSnapshotRequest checkpoint;checkpoint.expectedSemanticGeneration=before.semanticGeneration;
  checkpoint.documentRevision=7;checkpoint.recoveryFrontier={8,1,16,1,24,7};
  const auto saved=owner.captureDocumentSnapshot(checkpoint);assert(saved.status==runtime::DocumentSnapshotStatus::kCaptured);
  const auto observedBefore=owner.hostObservation();assert(observedBefore);
  const auto image=facade.exportImage(options);
  assert(image.status==runtime::ImageExportStatus::kExported&&image.width==600&&image.height==80);
  assert((image.bounds==std::array<double,4>{-100,20,500,100})&&image.generation==before.semanticGeneration);
  const auto pixels=decode(image);
  const auto at=[&](unsigned x,unsigned y,unsigned c){return pixels->rgba[(y*image.width+x)*4+c];};
  assert(at(10,10,0)==255&&at(10,10,2)==0&&at(590,10,2)==255&&at(590,10,0)==0);
  assert(at(300,10,0)==255&&at(300,10,1)==255);
  options.background=runtime::ImageExportBackground::kTransparent;
  const auto transparent=facade.exportImage(options);assert(transparent.status==runtime::ImageExportStatus::kExported);
  const auto alpha=decode(transparent);assert(alpha->rgba[(10*600+300)*4+3]==0&&alpha->rgba[(10*600+10)*4+3]==255);
  options.format=runtime::ImageExportFormat::kJpeg;
  assert(facade.exportImage(options).status==runtime::ImageExportStatus::kInvalidArgument);
  options.background=runtime::ImageExportBackground::kDocument;
  const auto jpeg=facade.exportImage(options);assert(jpeg.status==runtime::ImageExportStatus::kExported);
  assert(jpeg.bytes[0]==0xff&&jpeg.bytes[1]==0xd8&&decode(jpeg)->mimeType=="image/jpeg");
  options.scale=2;options.padding=8;
  const auto doubled=facade.exportImage(options);assert(doubled.width==1216&&doubled.height==176);
  options.scale=1e9;assert(facade.exportImage(options).status==runtime::ImageExportStatus::kTooLarge);
  options.scale=std::numeric_limits<double>::quiet_NaN();assert(facade.exportImage(options).status==runtime::ImageExportStatus::kInvalidArgument);
  options={};options.bounds=std::array<double,4>{0,0,5000,5000};
  const auto tooLarge=facade.exportImage(options);assert(tooLarge.status==runtime::ImageExportStatus::kTooLarge&&tooLarge.bytes.empty());
  options.bounds=std::array<double,4>{10,0,0,10};assert(facade.exportImage(options).status==runtime::ImageExportStatus::kInvalidArgument);
  options={};options.background=runtime::ImageExportBackground::kSolid;options.backgroundColor={0,1,0,1};options.padding=0;
  const auto green=decode(facade.exportImage(options));assert(green->rgba[(10*600+300)*4+1]==255&&green->rgba[(10*600+300)*4]==0);
  options={};options.expectedGeneration=before.semanticGeneration+1;
  assert(facade.exportImage(options).status==runtime::ImageExportStatus::kStale);
  runtime::ImageExportStatus other=runtime::ImageExportStatus::kExported;
  std::thread wrong([&]{other=facade.exportImage({}).status;});wrong.join();assert(other==runtime::ImageExportStatus::kWrongThread);
  const auto after=owner.captureDocumentSnapshot(checkpoint);
  assert(after.status==runtime::DocumentSnapshotStatus::kCaptured&&after.bytes==saved.bytes);
  const auto state=facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
  assert(state.semanticGeneration==before.semanticGeneration&&state.canUndo==before.canUndo);
  const auto observedAfter=owner.hostObservation();assert(observedAfter);
  assert(observedBefore->lastCanonicalFrame==observedAfter->lastCanonicalFrame&&
      observedBefore->lastOwnerControlCount==observedAfter->lastOwnerControlCount);
  PublicationProbe probe{&facade};
  assert(owner.setLocalOperationCallbacks(&PublicationProbe::applied,&probe,nullptr,nullptr,nullptr));
  runtime::CanvasControlRequest set{};set.clientId=99;set.requestId=1;
  set.target=facade.mountedCanvasTargets().front();set.payload.kind=runtime::CanvasControlPayloadKind::kDocumentBackground;
  set.payload.background.color={0,1,0,1};
  assert(facade.submitCanvasControl(set).state==runtime::ProductControlState::kQueued&&owner.ownerPump());
  assert(probe.result==runtime::ImageExportStatus::kBusy);
  assert(owner.close());assert(facade.exportImage({}).status==runtime::ImageExportStatus::kNotReady);

  runtime::RuntimeComposition inkOwner;assert(inkOwner.openDocument(document)&&inkOwner.bindSurface(256,256));
  semantic::ObjectRecord stroke;stroke.id=foundation::ObjectId::fromUint64(90);
  stroke.kind=semantic::ObjectKind::kVectorStroke;stroke.kind_version=1;stroke.placement.order_key=semantic::OrderKey{{1}};
  semantic::StrokeRecord data;data.brush.brush_family_id=1;data.brush.brush_version=1;
  data.brush.color={0,0,0,1};data.brush.nominal_size=10;data.brush.opacity=1;
  data.data=semantic::VectorStrokeData{{{{0,5},1,{}},{{40,5},1,{}}}};
  stroke.content=semantic::VectorStrokeContent{data};
  stroke.erase_masks.push_back({foundation::ObjectId::fromUint64(91),semantic::FilledPathMask{
      semantic::VectorPathGeometry{semantic::FillRule::kEvenOdd,{semantic::MoveTo{{16,0}},
        semantic::LineTo{{24,0}},semantic::LineTo{{24,10}},semantic::LineTo{{16,10}},semantic::ClosePath{}}}}});
  semantic::Operation inkInsert{semantic::OperationId{foundation::ObjectId::fromUint64(99)},
      semantic::DocumentId{document},1,1,semantic::InsertObjectsOp{{stroke}}};
  assert(inkOwner.applyRemoteOperation(inkInsert).disposition==semantic::ApplyDisposition::kApplied);
  options={};options.padding=0;options.bounds=std::array<double,4>{0,0,40,10};
  const auto erased=decode(inkOwner.controlFacade().exportImage(options));
  const auto offset=(5*40+20)*4;
  assert(erased->rgba[offset]==255&&erased->rgba[offset+1]==255&&erased->rgba[offset+3]==255);
  assert(erased->rgba[(5*40+5)*4]==0&&erased->rgba[(5*40+5)*4+3]==255);
  options.format=runtime::ImageExportFormat::kJpeg;
  const auto erasedJpeg=decode(inkOwner.controlFacade().exportImage(options));
  assert(erasedJpeg->rgba[offset]>240&&erasedJpeg->rgba[offset+1]>240);
  options.format=runtime::ImageExportFormat::kPng;options.background=runtime::ImageExportBackground::kTransparent;
  assert(decode(inkOwner.controlFacade().exportImage(options))->rgba[offset+3]==0);
  assert(inkOwner.close());
  // Optional fixture generation for the independent Web consumer. CTest never
  // writes the corpus; the fixture's expected pixels are literal checks above.
  if(argc==2) {
    const auto encoded=semantic::SemanticCodec::encodeProtobufOperation(inkInsert);assert(encoded.ok());
    std::ofstream output(argv[1],std::ios::binary);output.write(reinterpret_cast<const char*>(encoded.bytes.data()),static_cast<std::streamsize>(encoded.bytes.size()));assert(output.good());
  }
}
