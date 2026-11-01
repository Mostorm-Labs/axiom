// Isolated real-owner MSE probe. The existing D07 test observer merely arms
// a single allocation failure at a named participant; no production hook or
// fault policy is changed. Each invocation runs in its own process.
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/multi_selection_port.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>
#include <string_view>

namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {
 public:
  static bool select(interaction::CanvasEditingRuntime& runtime,
                     std::span<const foundation::ObjectId> ids) {
    return runtime.selection_.applyAcquisition(ids, interaction::SelectionCombineMode::kReplace,
                                                runtime.selection_.revision());
  }
  static auto& history(interaction::CanvasEditingRuntime& runtime) { return runtime.history_; }
  static auto& coordinator(interaction::CanvasEditingRuntime& runtime) { return *runtime.sceneCoordinator_; }
  static std::optional<RuntimeSceneRecord> scene(interaction::CanvasEditingRuntime& runtime,
                                                  foundation::ObjectId id) {
    const auto* record=runtime.sceneCoordinator_->runtimeScene().find(id);
    return record ? std::optional<RuntimeSceneRecord>{*record} : std::nullopt;
  }
};
}
namespace canvas {
class IncrementalRuntimeTestAccess {
 public:
  static void observeCheckpoints(IncrementalRuntimeCoordinator& coordinator,
      void (*observer)(void*, RuntimeCheckpoint) noexcept, void* context) {
    coordinator.testCheckpointObserver_=observer;
    coordinator.testCheckpointObserverContext_=context;
  }
};
}
namespace canvas::semantic::internal {
using IntentPrepareTestObserver = void (*)(void*, unsigned, std::size_t) noexcept;
void setIntentPrepareTestObserver(IntentPrepareTestObserver, void*) noexcept;
}
namespace {
bool armed = false;
bool injected = false;
struct SceneFault {
  static void observe(void*, canvas::RuntimeCheckpoint phase) noexcept {
    if (phase==static_cast<canvas::RuntimeCheckpoint>(0)) armed=true;
    if (phase==static_cast<canvas::RuntimeCheckpoint>(1)) armed=false;
  }
};
struct SemanticFault {
  unsigned begin = 100, end = 101;
  std::size_t operation = 0;
  static void observe(void* context, unsigned phase, std::size_t op) noexcept {
    const auto& fault = *static_cast<SemanticFault*>(context);
    if (op != fault.operation) return;
    if (phase == fault.begin) armed = true;
    if (phase == fault.end) armed = false;
  }
};
}
void* operator new(std::size_t size) {
  if (armed) { armed = false; injected = true; throw std::bad_alloc(); }
  if (auto* memory = std::malloc(size ? size : 1)) return memory;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main(int argc, char** argv) {
  using namespace canvas;
  if (argc != 2) return 2;
  const std::string_view mode{argv[1]};
  if (mode!="undo" && mode!="redo" && mode!="intent" && mode!="intent-fault" &&
      mode!="forward-semantic" && mode!="forward-scene") return 2;
  const bool redo = mode == "redo";
  const auto id=[](std::uint64_t n) { return foundation::ObjectId::fromUint64(n); };
  runtime::RuntimeCompositionCore composition;
  if (!composition.open() || !composition.bindSurface(800, 600) ||
      !composition.multiSelectionPort().setPermissionMode(
          runtime::MultiSelectionPermissionMode::kLocalSingleUser)) return 3;
  semantic::InsertObjectsOp seed;
  for (std::uint64_t i=1; i<=2; ++i) {
    semantic::ObjectRecord shape;
    shape.id=id(i); shape.kind=semantic::ObjectKind::kShape; shape.kind_version=1;
    shape.content=semantic::ShapeContent{1,40,30};
    shape.transform.tx=static_cast<double>(i*60);
    shape.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(i)});
    seed.objects.push_back(shape);
  }
  const semantic::Operation insert{semantic::OperationId{id(700)},semantic::DocumentId{id(1)},1,1,seed};
  if (composition.applyExternalOperation(insert,semantic::ApplySource::kRemoteSync).disposition !=
      semantic::ApplyDisposition::kApplied) return 4;
  auto& subject=composition.qualificationRuntime();
  const std::array roots{id(1),id(2)};
  if (!ink_playground::InkPlaygroundHistoryTestAccess::select(subject,roots)) return 5;
  if (mode=="intent" || mode=="intent-fault") {
    const auto beforeA=*subject.qualificationObjectRecord(id(1));
    const auto beforeB=*subject.qualificationObjectRecord(id(2));
    const auto beforeSceneA=ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(1));
    const auto beforeSceneB=ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(2));
    if (!beforeSceneA || !beforeSceneB) return 14;
    const auto before=composition.readback();
    auto& history=ink_playground::InkPlaygroundHistoryTestAccess::history(subject);
    auto transformA=beforeA.transform, transformB=beforeB.transform;
    transformA.tx+=20; transformB.tx+=20;
    const semantic::Operation move{semantic::OperationId{id(710)},semantic::DocumentId{id(1)},1,1,
        semantic::SetTransformsOp{{{id(1),transformA},{id(2),transformB}}}};
    const semantic::Operation properties{semantic::OperationId{id(711)},semantic::DocumentId{id(1)},1,1,
        semantic::PatchPropertiesOp{{{id(1),3,semantic::PropertyPatchAction::kSet,semantic::PropertyValue{0.5F}},
                                      {id(2),3,semantic::PropertyPatchAction::kSet,semantic::PropertyValue{0.5F}}}}};
    const std::array ops{move,properties};
    if (mode=="intent-fault") {
      const auto size=history.size(); const bool undo=history.canUndo(), redoBefore=history.canRedo();
      SemanticFault fault;
      semantic::internal::setIntentPrepareTestObserver(&SemanticFault::observe,&fault);
      const bool began=subject.beginExplicitIntent();
      const bool prepared=began && subject.prepareExplicitIntent(ops);
      armed=false; semantic::internal::setIntentPrepareTestObserver(nullptr,nullptr);
      subject.abortExplicitIntent();
      const auto after=composition.readback();
      if (!began || prepared || !injected || *subject.qualificationObjectRecord(id(1))!=beforeA ||
          *subject.qualificationObjectRecord(id(2))!=beforeB ||
          ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(1))!=beforeSceneA ||
          ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(2))!=beforeSceneB ||
          after.semanticGeneration!=before.semanticGeneration || after.sceneRevision!=before.sceneRevision ||
          history.size()!=size || history.canUndo()!=undo || history.canRedo()!=redoBefore) return 15;
    }
    if (!subject.beginExplicitIntent() || !subject.prepareExplicitIntent(ops) ||
        !subject.commitExplicitIntent() || history.size()!=1U) return 16;
    const auto afterA=*subject.qualificationObjectRecord(id(1));
    const auto afterB=*subject.qualificationObjectRecord(id(2));
    const auto hasOpacity=[](const semantic::ObjectRecord& record) {
      return std::any_of(record.properties.entries.begin(),record.properties.entries.end(),
          [](const auto& entry) {
            const auto* value=std::get_if<float>(&entry.value);
            return entry.field_id==3U && value && *value==0.5F;
          });
    };
    if (afterA.transform.tx!=transformA.tx || afterB.transform.tx!=transformB.tx ||
        !hasOpacity(afterA) || !hasOpacity(afterB)) return 17;
    if (!subject.undo() || *subject.qualificationObjectRecord(id(1))!=beforeA ||
        *subject.qualificationObjectRecord(id(2))!=beforeB || history.canUndo() || !history.canRedo()) return 18;
    if (!subject.redo() || *subject.qualificationObjectRecord(id(1))!=afterA ||
        *subject.qualificationObjectRecord(id(2))!=afterB || !history.canUndo()) return 19;
    std::fprintf(stderr,"MSE_MULTI_INTENT mode=%s injected=%d single_undo_redo=1\n",argv[1],injected);
    return 0;
  }
  auto& port=composition.multiSelectionPort();
  const auto projection=port.read();
  if (!projection.available || !projection.properties.contains(3U)) return 6;
  const auto eligible=projection.properties.at(3U).eligible;
  if (eligible.size()!=2U) return 6;
  if (mode=="forward-semantic" || mode=="forward-scene") {
    const auto beforeA=*subject.qualificationObjectRecord(id(1));
    const auto beforeB=*subject.qualificationObjectRecord(id(2));
    const auto sceneA=ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(1));
    const auto sceneB=ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(2));
    if (!sceneA || !sceneB) return 20;
    const auto before=composition.readback();
    auto& history=ink_playground::InkPlaygroundHistoryTestAccess::history(subject);
    const auto size=history.size();const bool canUndo=history.canUndo(),canRedo=history.canRedo();
    if (!port.submitProperty(3U,semantic::PropertyValue{0.5F},projection.stamp,eligible,7042,1).queued) return 21;
    SemanticFault fault;
    auto& coordinator=ink_playground::InkPlaygroundHistoryTestAccess::coordinator(subject);
    if (mode=="forward-scene") IncrementalRuntimeTestAccess::observeCheckpoints(coordinator,&SceneFault::observe,nullptr);
    else semantic::internal::setIntentPrepareTestObserver(&SemanticFault::observe,&fault);
    const bool pump=composition.ownerPump();
    armed=false;
    IncrementalRuntimeTestAccess::observeCheckpoints(coordinator,nullptr,nullptr);
    semantic::internal::setIntentPrepareTestObserver(nullptr,nullptr);
    const auto after=composition.readback(); const auto failed=port.receipt(7042,1);
    bool safe=pump && injected && failed && failed->terminal && !failed->applied &&
      *subject.qualificationObjectRecord(id(1))==beforeA && *subject.qualificationObjectRecord(id(2))==beforeB &&
      ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(1))==sceneA &&
      ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(2))==sceneB &&
      after.semanticGeneration==before.semanticGeneration && after.sceneRevision==before.sceneRevision &&
      history.size()==size && history.canUndo()==canUndo && history.canRedo()==canRedo;
    if (safe) {
      const auto next=port.read();
      safe=next.available && port.submitProperty(3U,semantic::PropertyValue{0.5F},next.stamp,
              next.properties.at(3U).eligible,7042,2).queued && composition.ownerPump();
      const auto applied=port.receipt(7042,2);
      safe=safe && applied && applied->applied && applied->changed.size()==2U && history.size()==size+1U;
    }
    std::fprintf(stderr,"MSE_FORWARD mode=%s injected=%d unchanged_and_retry=%d\n",argv[1],injected,safe);
    return safe ? 0 : 22;
  }
  if (!port.submitProperty(3U,semantic::PropertyValue{0.5F},
          projection.stamp,eligible,7040,1).queued || !composition.ownerPump()) return 7;
  const auto outcome=port.receipt(7040,1);
  if (!outcome || !outcome->applied || outcome->changed.size()!=2U) return 8;
  const auto opacity=[&](foundation::ObjectId object) -> std::optional<float> {
    const auto* record=subject.qualificationObjectRecord(object);
    if (!record) return std::nullopt;
    for (const auto& entry:record->properties.entries)
      if (entry.field_id==3U) {
        if (const auto* value=std::get_if<float>(&entry.value)) return *value;
        return std::nullopt;
      }
    return 1.0F;
  };
  if (opacity(id(1))!=0.5F || opacity(id(2))!=0.5F) return 12;
  auto& facade=composition.controlFacade();
  const auto queue=[&](bool doRedo,std::uint64_t request) {
    const auto epoch=facade.readRuntimeState().identity.runtimeGeneration;
    return doRedo ? facade.redo(request,epoch,7041) : facade.undo(request,epoch,7041);
  };
  if (redo && (queue(false,1).state!=runtime::ProductControlState::kQueued ||
               !composition.ownerPump() || opacity(id(1))!=1.0F || opacity(id(2))!=1.0F)) return 9;
  auto& history=ink_playground::InkPlaygroundHistoryTestAccess::history(subject);
  const auto a=*subject.qualificationObjectRecord(id(1));
  const auto b=*subject.qualificationObjectRecord(id(2));
  const auto state=composition.readback();
  const auto sceneA=ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(1));
  const auto sceneB=ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(2));
  if (!sceneA || !sceneB) return 13;
  const auto size=history.size(); const bool canUndo=history.canUndo(),canRedo=history.canRedo();
  if (queue(redo,2).state!=runtime::ProductControlState::kQueued) return 10;
  SemanticFault fault;
  semantic::internal::setIntentPrepareTestObserver(&SemanticFault::observe,&fault);
  const bool pump=composition.ownerPump();
  armed=false; semantic::internal::setIntentPrepareTestObserver(nullptr,nullptr);
  const auto after=composition.readback();
  const auto failedReceipt=facade.canvasControlReceipt({7041,2});
  bool safe=injected && pump && failedReceipt &&
      failedReceipt->state!=runtime::CanvasControlReceiptState::kQueued &&
      failedReceipt->state!=runtime::CanvasControlReceiptState::kApplied && *subject.qualificationObjectRecord(id(1))==a &&
      *subject.qualificationObjectRecord(id(2))==b &&
      after.semanticGeneration==state.semanticGeneration && after.sceneRevision==state.sceneRevision &&
      ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(1))==sceneA &&
      ink_playground::InkPlaygroundHistoryTestAccess::scene(subject,id(2))==sceneB &&
      history.size()==size && history.canUndo()==canUndo && history.canRedo()==canRedo;
  if (safe) safe=queue(redo,3).state==runtime::ProductControlState::kQueued && composition.ownerPump();
  const auto retriedReceipt=facade.canvasControlReceipt({7041,3});
  if (safe) safe=retriedReceipt && retriedReceipt->state==runtime::CanvasControlReceiptState::kApplied &&
      opacity(id(1))==(redo?0.5F:1.0F) && opacity(id(2))==(redo?0.5F:1.0F) &&
      (redo ? history.canUndo() : history.canRedo());
  std::fprintf(stderr,"MSE_HISTORY direction=%s injected=%d unchanged_and_retry=%d\n",argv[1],injected,safe);
  return safe ? 0 : 11;
}
