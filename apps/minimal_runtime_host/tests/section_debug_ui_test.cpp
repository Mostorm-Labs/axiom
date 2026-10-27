#include "minimal_runtime_facade.hpp"
#include "canvas/debug_ui/controller.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <cassert>
#include <iostream>
#include <string_view>

int main() {
  using namespace canvas;
  minimal_runtime_host::MinimalRuntimeHost host;
  assert(host.composition().bindSurface(800,600));
  minimal_runtime_host::MinimalRuntimeFacade hostFacade(host);
  runtime::RuntimeFacade& facade=hostFacade; // same wrapper the Windows window mounts
  const auto canonicalPixels=[&] {
    runtime::RuntimeFrameOpportunity frame;
    assert(host.composition().serviceFrameOpportunity(true,frame));
    std::vector<std::uint8_t> rgba(800*600*4);
    assert(host.composition().readCanonicalRgba(rgba));
    return rgba;
  };
  const auto emptyCanvas=canonicalPixels();
  ImGui::CreateContext();
  auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,1600};io.DeltaTime=1.0F/60;
  unsigned char* pixels=nullptr;int width=0,height=0;
  io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
  debug_ui::DebugControllerContext context{};context.runtime=&facade;
  debug_ui::DebugController controller(context);
  controller.uiState().setWorkspace(debug_ui::WorkspaceId::kControl);
  const auto draw=[&] {ImGui::NewFrame();(void)controller.buildImGuiFrame();ImGui::Render();};
  const auto activate=[&](const char* label) {
    for(int i=0;i<3;++i)draw();
    ImGuiWindow* content=nullptr;
    for(auto* window:ImGui::GetCurrentContext()->Windows)
      if(std::string_view(window->Name).find("/##content_")!=std::string_view::npos)content=window;
    assert(content);
    ImGui::NewFrame();auto& gui=*ImGui::GetCurrentContext();
    gui.NavActivateId=gui.NavActivateDownId=content->GetID(label);
    gui.NavInputSource=ImGuiInputSource_Keyboard;
    const bool submitted=controller.buildImGuiFrame();ImGui::Render();
    gui.NavActivateId=gui.NavActivateDownId=0;ImGui::ClearActiveID();
    assert(submitted);assert(host.composition().ownerPump());draw();
  };
  activate("Create Section");
  for(const auto& entry:controller.snapshot().activity.entries)if(entry.productReceipt&&entry.productReceipt->control) {
    const auto& receipt=*entry.productReceipt->control;
    std::cout<<"request="<<receipt.key.requestId<<" state="<<static_cast<unsigned>(receipt.state);
    if(receipt.section)std::cout<<" section_failure="<<static_cast<unsigned>(receipt.section->failure);
    std::cout<<'\n';
    if(const auto actual=facade.canvasControlReceipt(receipt.key)) {
      std::cout<<"owner_state="<<static_cast<unsigned>(actual->state);
      if(actual->section)std::cout<<" owner_section_failure="<<static_cast<unsigned>(actual->section->failure);
      std::cout<<'\n';
    }
  }
  if(host.composition().semanticObjectCount()!=1) {
    std::cerr<<"FAIL: actual Create Section widget must create one canonical object; got "
             <<host.composition().semanticObjectCount()<<'\n';
    ImGui::DestroyContext();return 1;
  }
  const auto section=facade.readSection();
  assert(section.available&&section.projectionReady&&section.selectionRevision!=0);
  assert(section.target==facade.mountedCanvasTargets().front());
  assert(section.content.boundary.width==320&&section.content.boundary.height==240);
  assert(section.selectedRoots==std::vector{section.objectId});
  assert(canonicalPixels()!=emptyCanvas); // a real visible Section, not only a receipt
  const auto activity=controller.snapshot().activity;
  assert(!activity.entries.empty()&&activity.entries.back().state==debug_ui::DebugActivityState::kApplied);
  activate("Release Section");assert(host.composition().semanticObjectCount()==0);
  assert(canonicalPixels()==emptyCanvas);
  activate("Undo (Ctrl+Z)");assert(host.composition().semanticObjectCount()==1);
  assert(facade.readSection(section.objectId).available);
  activate("Redo (Ctrl+Y)");assert(host.composition().semanticObjectCount()==0);
  // The same visible panel must be able to read selection and wrap real objects.
  runtime::StickyCreateRequest sticky;sticky.objectId=foundation::ObjectId::fromUint64(6090);
  sticky.content={80,80};sticky.transform.tx=100;sticky.transform.ty=120;
  assert(facade.createSticky(sticky).state==runtime::ConnectorCommandState::kApplied);
  assert(facade.setSelectionMode(true,1,facade.readRuntimeState().identity.runtimeGeneration,6090).state==runtime::ProductControlState::kQueued);
  assert(host.composition().ownerPump());assert(host.registerSource(6090));
  const auto view=facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
  for(std::uint64_t i=1;i<=2;++i) {
    input::PlatformPointerBatch batch;
    batch.samples.push_back({6090,1,i,i*1000000,140*view.cameraScale+view.cameraTranslationX,
      160*view.cameraScale+view.cameraTranslationY,0,0,0,{},{},input::SampleProvenance::kConfirmedCurrent,
      i==1?input::PointerPhase::kDown:input::PointerPhase::kUp});
    assert(host.submit(batch));
  }
  assert(facade.readRuntimeState().selection.selectedObjectCount==1);
  activate("Wrap selection");assert(host.composition().semanticObjectCount()==2);
  const auto wrapped=facade.readSection();assert(wrapped.available&&wrapped.directChildren==std::vector{sticky.objectId});
  assert(wrapped.members.front().worldTransform.tx==100&&wrapped.members.front().worldTransform.ty==120);
  activate("Release Section");assert(host.composition().semanticObjectCount()==1);
  assert(!facade.readSection(wrapped.objectId).available);
  const auto roots=facade.readSection().selectedRoots;
  activate("Rectangle select");
  assert(facade.readRuntimeState().control.selectionAcquisitionMode==runtime::CanvasSelectionMode::kRectangle);
  assert(facade.readSection().selectedRoots==roots);
  activate("Freeform lasso");
  assert(facade.readRuntimeState().control.selectionAcquisitionMode==runtime::CanvasSelectionMode::kFreeform);
  activate("Hand");assert(facade.readRuntimeState().tool.toolId==3);
  assert(facade.readSection().selectedRoots==roots);
  activate("Select");assert(facade.readRuntimeState().control.selectionAcquisitionMode==runtime::CanvasSelectionMode::kPoint);
  assert(facade.readSection().selectedRoots==roots);
  ImGui::DestroyContext();
  std::cout<<"PASS: real Section widgets through MinimalRuntimeFacade create/wrap/release and Undo/Redo\n";
}
