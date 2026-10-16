#include "ink_playground_host.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <sstream>

namespace canvas::ink_playground {
bool InkPlaygroundHost::configureTextResources(std::vector<std::uint8_t> latin,
    std::vector<std::uint8_t> cjk,text::LayoutContext context) {
  if(latin.empty() || cjk.empty() || !std::isfinite(context.width) || context.width<=0) return false;
  latinTextFont_=std::move(latin); cjkTextFont_=std::move(cjk);
  textResources_.publish(foundation::ObjectId::fromUint64(0x470001),latinTextFont_);
  textResources_.publish(foundation::ObjectId::fromUint64(0x470002),cjkTextFont_);
  textLayoutService_=std::make_unique<text::RichTextLayoutService>(textResources_,context);
  textLayoutService_->registerFont({{foundation::ObjectId::fromUint64(0x470001)},std::string(text::kRobotoSha256),0});
  textLayoutService_->registerFont({{foundation::ObjectId::fromUint64(0x470002)},std::string(text::kNotoSha256),0});
  sceneCoordinator_->setTextLayoutService(textLayoutService_.get());
  return true;
}
bool InkPlaygroundHost::seedTextScenario(std::string_view scenario,std::size_t count) {
  if(!textLayoutService_ || count==0 || !historySceneReady() ||
     !brushSessions_.empty() || !eraserTraces_.empty() || !keyedStrokeIds_.empty()) return false;
  const std::array<std::string_view,8> scenarios{"text-many-small","text-long","text-style-mixed","text-edit-local",
    "text-font-cold-warm","text-transform","text-camera","structured-grid-proxy"};
  if(std::find(scenarios.begin(),scenarios.end(),scenario)==scenarios.end()) return false;
  if(count>50000 || textScenarioSerial_>=100000) return false;
  if(!textScenarioFixtureObjects_.empty()) {
    clearSelection();
    if(!submitTextScenarioOperation(semantic::DeleteObjectsOp{textScenarioFixtureObjects_})) return false;
  }
  const auto idBase=0x480000ULL+(++textScenarioSerial_)*0x200000ULL;
  semantic::InsertObjectsOp payload; payload.objects.reserve(count*2);
  std::vector<foundation::ObjectId> textIds,fixtureIds;
  const semantic::TextStyle latin{semantic::ResourceId{foundation::ObjectId::fromUint64(0x470001)},16,400,false,false,{0.08F,0.13F,0.19F,1}};
  const semantic::TextStyle cjk{semantic::ResourceId{foundation::ObjectId::fromUint64(0x470002)},16,400,false,false,{0.05F,0.35F,0.45F,1}};
  for(std::size_t i=0;i<count;++i) {
    semantic::ObjectRecord object; object.id=foundation::ObjectId::fromUint64(idBase+i*2+1);
    object.kind=semantic::ObjectKind::kRichText; object.kind_version=1;
    object.placement.order_key=semantic::OrderKey{{static_cast<std::uint8_t>(1+i/(254*254)),
      static_cast<std::uint8_t>(1+(i/254)%254),static_cast<std::uint8_t>(1+i%254)}};
    object.transform.tx=12+static_cast<double>((i%8)*160); object.transform.ty=12+static_cast<double>((i/8)*36);
    std::string value="Canvas "+std::to_string(i)+" A\xcc\x81";
    if(scenario=="text-long") for(int j=0;j<12;++j) value+=" punctuation 0123456789, line wrapping. ";
    semantic::Paragraph paragraph{foundation::ObjectId::fromUint64(idBase+0x100000+i),
      {semantic::ParagraphAlignment::kLeft,1.2,0,0},{{value,latin}}};
    if(scenario=="text-style-mixed" || scenario=="structured-grid-proxy") {
      auto underlined=latin; underlined.underline=true; underlined.color={0.6F,0.1F,0.2F,1};
      // The pinned CJK subset has the CJK glyph, but no space glyph.
      // Assign each run to its explicit resource instead of invoking fallback.
      paragraph.runs.front().text += " ";
      paragraph.runs.push_back({"\xe6\x98\xaf",cjk}); paragraph.runs.push_back({" underline",underlined});
    }
    object.content=semantic::RichTextContent{{{paragraph}}}; textIds.push_back(object.id); fixtureIds.push_back(object.id);
    payload.objects.push_back(object);
    if(scenario=="structured-grid-proxy") {
      object.id=foundation::ObjectId::fromUint64(idBase+i*2+2); object.kind=semantic::ObjectKind::kShape;
      object.content=semantic::ShapeContent{1,144,28}; object.transform.ty+=24;
      object.placement.order_key=semantic::OrderKey{{1,static_cast<std::uint8_t>(1+(i/254)%254),
        static_cast<std::uint8_t>(1+i%254),1}};
      fixtureIds.push_back(object.id);
      payload.objects.push_back(std::move(object));
    }
  }
  if(!submitTextScenarioOperation(std::move(payload))) return false;
  textScenarioObjects_=std::move(textIds); textScenarioFixtureObjects_=std::move(fixtureIds);
  textScenarioName_=scenario;
  return true;
}
bool InkPlaygroundHost::submitTextScenarioOperation(semantic::OperationPayload payload) {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  const auto ordinal=localOperationOrdinal(); if(!ordinal) return false;
  semantic::Operation operation{semantic::OperationId(foundation::ObjectId::fromUint64(ordinal)),documentId_,1,1,std::move(payload)};
  const auto applied=operationEngine_.apply(operation,semantic::ApplySource::kLocalCommand,semanticObjects_,
    appliedOperations_,semanticGeneration_,canonicalCommitClock_);
  if(applied.disposition!=semantic::ApplyDisposition::kApplied || !applied.commit_record) return false;
  const semantic::SemanticReadView view(semanticObjects_,semanticGeneration_.current());
  const auto& changes=applied.commit_record->change_set;
  if(!sceneCoordinator_->apply(*sceneCompiler_,canvas::SceneCommitInput(changes.beforeGeneration(),changes.afterGeneration(),view,&changes))) return false;
  ++submittedOperationCount_; canonicalPresentationDirty_=true; return true;
}
bool InkPlaygroundHost::editTextScenario(std::size_t index) {
  return applyTextScenarioEdit(index,"insert");
}
bool InkPlaygroundHost::applyTextScenarioEdit(std::size_t index,std::string_view action) {
  if(index>=textScenarioObjects_.size() || !historySceneReady()) return false;
  const auto objectId=textScenarioObjects_[index]; const auto* object=semanticObjects_.find(objectId);
  if(!object) return false;
  const auto& paragraph=std::get<semantic::RichTextContent>(object->content).document.paragraphs.front();
  semantic::RichTextStep step;
  if(action=="insert") step=semantic::InsertTextStep{paragraph.id,0,"!",paragraph.runs.front().style};
  else if(action=="delete") step=semantic::DeleteTextStep{paragraph.id,0,1};
  else if(action=="split") step=semantic::SplitParagraphStep{paragraph.id,1,
    foundation::ObjectId::fromUint64(0x790000000000ULL+nextHistoryOperationOrdinal_)};
  else if(action=="merge") {
    const auto& paragraphs=std::get<semantic::RichTextContent>(object->content).document.paragraphs;
    if(paragraphs.size()<2) return false;
    step=semantic::MergeParagraphStep{paragraph.id,paragraphs[1].id};
  } else if(action=="inline-style") {
    auto style=paragraph.runs.front().style; style.underline=!style.underline;
    step=semantic::SetInlineStyleStep{paragraph.id,0,1,style};
  } else if(action=="paragraph-style") {
    auto style=paragraph.style;
    style.alignment=style.alignment==semantic::ParagraphAlignment::kCenter
        ?semantic::ParagraphAlignment::kLeft:semantic::ParagraphAlignment::kCenter;
    step=semantic::SetParagraphStyleStep{paragraph.id,style};
  } else return false;
  return submitTextScenarioOperation(semantic::EditRichTextOp{objectId,semantic::RichTextDelta{1,{std::move(step)}}});
}
bool InkPlaygroundHost::transformTextScenario(std::size_t index) {
  if(index>=textScenarioObjects_.size() || !historySceneReady()) return false;
  const auto* object=semanticObjects_.find(textScenarioObjects_[index]); if(!object) return false;
  auto transform=object->transform; transform.tx+=5; transform.a*=1.05; transform.d*=1.05;
  return submitTextScenarioOperation(semantic::SetTransformsOp{{{object->id,transform}}});
}
std::string InkPlaygroundHost::textQualificationJson() const {
  const auto metrics=textLayoutMetrics(); std::size_t glyphs=0,diagnostics=0;
  std::ostringstream out; out.precision(17);
  out<<"{\"scenario\":\""<<textScenarioName_<<"\",\"objects\":"<<textScenarioObjects_.size()
     <<",\"semantic_generation\":"<<semanticGeneration_.current().value()
     <<",\"scene_revision\":"<<sceneRevision()<<",\"canonical_operations\":"<<submittedOperationCount_
     <<",\"object_layouts\":"<<metrics.objectLayouts<<",\"paragraph_layouts\":"<<metrics.paragraphLayouts
     <<",\"cache_hits\":"<<metrics.cacheHits<<",\"font_materializations\":"<<metrics.fontMaterializations
     <<",\"layout_cpu_ms\":"<<metrics.layoutCpuMs<<",\"layout_digests\":[";
  bool first=true;
  for(const auto& record:sceneCoordinator_->runtimeScene().records()) if(record.textLayout) {
    if(!first) out<<','; first=false; out<<'\"'<<record.textLayout->digest<<'\"';
    for(const auto& run:record.textLayout->runs) glyphs+=run.glyphs.size();
    diagnostics+=record.textLayout->diagnostics.size();
  }
  out<<"],\"glyphs\":"<<glyphs<<",\"diagnostics\":"<<diagnostics<<",\"profile\":\""<<text::kTextStackProfile<<"\"}";
  return out.str();
}
bool InkPlaygroundHost::setTextFontsAvailable(bool available) {
  if(!textLayoutService_) return false;
  if(available) {
    textResources_.publish(foundation::ObjectId::fromUint64(0x470001),latinTextFont_);
    textResources_.publish(foundation::ObjectId::fromUint64(0x470002),cjkTextFont_);
  } else {
    textResources_.markMissing(foundation::ObjectId::fromUint64(0x470001));
    textResources_.markMissing(foundation::ObjectId::fromUint64(0x470002));
  }
  const auto refreshed=sceneCoordinator_->refreshTextResources(); canonicalPresentationDirty_=true;
  return static_cast<bool>(refreshed);
}
text::LayoutMetrics InkPlaygroundHost::textLayoutMetrics() const noexcept {
  return textLayoutService_ ? textLayoutService_->metrics() : text::LayoutMetrics{};
}

bool InkPlaygroundHost::seedQualificationFixture(std::size_t objectCount) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  if (objectCount == 0U || !brushSessions_.empty() || !eraserTraces_.empty() ||
      !keyedStrokeIds_.empty() || !historySceneReady()) return false;
  const auto ordinal = localOperationOrdinal();
  if (ordinal == 0U) return false;
  semantic::InsertObjectsOp payload;
  payload.objects.reserve(objectCount);
  for (std::size_t index = 0; index < objectCount; ++index) {
    semantic::ObjectRecord object;
    object.id = foundation::ObjectId::fromUint64(0x400000ULL + index + 1U);
    object.kind = semantic::ObjectKind::kShape;
    object.kind_version = 1U;
    object.placement.order_key = semantic::OrderKey{
        {static_cast<std::uint8_t>(1U + (index / (254U * 254U))),
         static_cast<std::uint8_t>(1U + ((index / 254U) % 254U)),
         static_cast<std::uint8_t>(1U + (index % 254U))}};
    object.transform.tx = static_cast<double>((index % 8U) * 32U);
    object.transform.ty = static_cast<double>((index / 8U) * 32U);
    object.content = semantic::ShapeContent{1U, 24.0, 24.0};
    payload.objects.push_back(std::move(object));
  }
  // A qualification corpus is authored in legal Operation batches. The
  // structural 65535 item limit remains unchanged for actual user intents.
  constexpr std::size_t kFixtureBatchSize = 50000;
  for (std::size_t offset=0;offset<payload.objects.size();offset+=kFixtureBatchSize) {
    const auto end=std::min(offset+kFixtureBatchSize,payload.objects.size());
    semantic::InsertObjectsOp batch;
    batch.objects.insert(batch.objects.end(),payload.objects.begin()+offset,payload.objects.begin()+end);
    const auto operationOrdinal=localOperationOrdinal();
    if(operationOrdinal==0) return false;
    const semantic::Operation operation{
        semantic::OperationId(foundation::ObjectId::fromUint64(operationOrdinal)),
        documentId_,1,1,std::move(batch)};
    const auto applied=operationEngine_.apply(operation,semantic::ApplySource::kLocalCommand,
        semanticObjects_,appliedOperations_,semanticGeneration_,canonicalCommitClock_);
    if(applied.disposition!=semantic::ApplyDisposition::kApplied || !applied.commit_record) return false;
    ++submittedOperationCount_;
  }
  const semantic::SemanticReadView view(semanticObjects_,semanticGeneration_.current());
  const canvas::SceneCommitInput recovery(semanticGeneration_.current(),view);
  if(!sceneCoordinator_->recover(*sceneCompiler_,recovery)) return false;
  const auto queryStart = std::chrono::steady_clock::now();
  const auto query = runtimeSceneHost_->query(canvas::SceneQuery{
      foundation::WorldRect{0.0F, 0.0F, static_cast<float>(surface_.width),
                            static_cast<float>(surface_.height)}});
  const auto queryEnd = std::chrono::steady_clock::now();
  if (!query) return false;
  qualificationObservation_.sceneGeneration = runtimeSceneHost_->revision().value();
  qualificationObservation_.cameraGeneration = cameraGeneration();
  qualificationObservation_.candidatesExamined = query.value().diagnostics.candidatesExamined;
  const auto& invalidation = runtimeSceneHost_->invalidationOutput();
  qualificationObservation_.invalidationRectCount = invalidation.rects.size();
  qualificationObservation_.fullSceneInvalidation = invalidation.fullScene;
  qualificationObservation_.queryMs = std::chrono::duration<double, std::milli>(
      queryEnd - queryStart).count();
  qualificationObservation_.canonicalOperationCount = submittedOperationCount_;
  const auto renderStart = std::chrono::steady_clock::now();
  const bool rendered = presentCanonicalFrame(canonicalFrameCount_ + 1U, 0.0, false);
  const auto renderEnd = std::chrono::steady_clock::now();
  if (!rendered) return false;
  qualificationObservation_.renderMs = std::chrono::duration<double, std::milli>(
      renderEnd - renderStart).count();
  canonicalPresentationDirty_ = true;
  return true;
}


}  // namespace canvas::ink_playground
