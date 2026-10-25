#include "minimal_runtime_facade.hpp"
#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <string_view>
#include <fstream>
#include <iterator>
#include <vector>
#include <windows.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <filesystem>
#include <array>

namespace {
std::filesystem::path filePickerSelection;
BOOL WINAPI fixtureGetOpenFileNameW(LPOPENFILENAMEW dialog) {
  // Exercise the real picker configuration and loader, replacing only the
  // interactive OS call. Default browsing must expose valid images even if
  // their suffix is absent or not understood by Explorer's extension filter.
  const wchar_t* pattern = dialog->lpstrFilter;
  const DWORD selectedFilter = dialog->nFilterIndex == 0 ? 1 : dialog->nFilterIndex;
  for (DWORD i = 1; i < selectedFilter; ++i) {
    pattern += std::wcslen(pattern) + 1;
    pattern += std::wcslen(pattern) + 1;
  }
  pattern += std::wcslen(pattern) + 1;
  assert(PathMatchSpecW(L"test.jpg", pattern));
  assert(PathMatchSpecW(L"test.jpeg", pattern));
  assert(PathMatchSpecW(L"test.JPG", pattern));
  assert(PathMatchSpecW(L"\u56fe\u7247.png", pattern));
  assert(PathMatchSpecW(L"image_without_suffix", pattern));
  assert(wcscpy_s(dialog->lpstrFile, dialog->nMaxFile, filePickerSelection.c_str()) == 0);
  return TRUE;
}
} // namespace

#define GetOpenFileNameW fixtureGetOpenFileNameW
#include "windows_image_file.hpp"
#undef GetOpenFileNameW

namespace {
void testNativeImageFileLoading() {
  std::array<wchar_t, MAX_PATH> temp{};
  assert(GetTempPathW(static_cast<DWORD>(temp.size()), temp.data()) != 0);
  const auto root = std::filesystem::path(temp.data()) /
      (L"axiom-image-picker-" + std::to_wstring(GetCurrentProcessId()));
  assert(std::filesystem::create_directory(root));
  const auto fixture = std::filesystem::path(FC03_FIXTURE_ROOT) / "fc03-sample.jpg";
  std::ifstream input(fixture, std::ios::binary);
  const std::vector<std::uint8_t> expected{std::istreambuf_iterator<char>(input), {}};
  assert(!expected.empty());
  for (const wchar_t* name : {L"image.jpg", L"image.jpeg", L"\u56fe\u7247.JPG", L"image_without_suffix"}) {
    filePickerSelection = root / name;
    std::filesystem::copy_file(fixture, filePickerSelection);
    const auto result = canvas::minimal_runtime_host::chooseWindowsImageFile(nullptr);
    assert(result.error.empty() && !result.cancelled);
    assert(result.mimeType == "image/jpeg" && result.bytes == expected);
    assert(std::filesystem::remove(filePickerSelection));
  }
  filePickerSelection = root / L"not-an-image.jpeg";
  { std::ofstream invalid(filePickerSelection, std::ios::binary); invalid << "not an image"; }
  const auto invalid = canvas::minimal_runtime_host::chooseWindowsImageFile(nullptr);
  assert(!invalid.error.empty() && invalid.bytes.empty());
  assert(std::filesystem::remove(filePickerSelection));
  filePickerSelection = root / L"jpeg-labelled-as-png.png";
  std::filesystem::copy_file(fixture, filePickerSelection);
  const auto mismatched = canvas::minimal_runtime_host::chooseWindowsImageFile(nullptr);
  assert(!mismatched.error.empty() && mismatched.bytes.empty());
  assert(std::filesystem::remove(filePickerSelection));
  assert(std::filesystem::remove(root));
}
} // namespace

int main() {
  using namespace canvas;
  testNativeImageFileLoading();
  minimal_runtime_host::MinimalRuntimeHost host;
  assert(host.composition().bindSurface(800, 600));
  minimal_runtime_host::MinimalRuntimeFacade facade(host);
  ImGui::CreateContext();
  auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = {900, 1400}; io.DeltaTime = 1.0F / 60;
  unsigned char* pixels = nullptr; int w = 0, h = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
  debug_ui::DebugControllerContext context{};
  context.runtime = &facade;
  bool cancelFile = false;
  const char* fileName = "fc03-sample.jpg";
  std::string mime = "image/jpeg";
  context.chooseImageFile = [&] {
    debug_ui::ImageFileResult result;
    if (cancelFile) { result.cancelled = true; return result; }
    std::ifstream file(std::string(FC03_FIXTURE_ROOT) + "/" + fileName, std::ios::binary);
    result.bytes = {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    result.mimeType = mime;
    return result;
  };
  debug_ui::DebugController controller(context);
  controller.uiState().setWorkspace(debug_ui::WorkspaceId::kControl);
  for (int i = 0; i < 3; ++i) { ImGui::NewFrame(); (void)controller.buildImGuiFrame(); ImGui::Render(); }
  ImGuiWindow* content = nullptr;
  for (auto* window : ImGui::GetCurrentContext()->Windows)
    if (std::string_view(window->Name).find("/##content_") != std::string_view::npos) content = window;
  assert(content);
  ImGui::NewFrame();
  auto& gui = *ImGui::GetCurrentContext();
  gui.NavActivateId = gui.NavActivateDownId = content->GetID("Import image...");
  gui.NavInputSource = ImGuiInputSource_Keyboard;
  const bool submitted = controller.buildImGuiFrame();
  ImGui::Render();
  assert(submitted);  // Old Control panel has no Image action at all.
  controller.processHostRequests();
  assert(host.composition().semanticObjectCount() == 1);
  const auto imported = facade.readSelectedImage();
  assert(imported.available && imported.editable);
  assert(imported.content.intrinsic_width == 138 && imported.content.intrinsic_height == 138);
  const auto activate = [&](const char* label) {
    for (int i=0;i<2;++i) { ImGui::NewFrame(); (void)controller.buildImGuiFrame(); ImGui::Render(); }
    ImGui::NewFrame(); gui.NavActivateId = gui.NavActivateDownId = content->GetID(label);
    gui.NavInputSource = ImGuiInputSource_Keyboard;
    assert(controller.buildImGuiFrame()); ImGui::Render();
    controller.processHostRequests();
  };
  cancelFile = true;
  activate("Replace selected image...");
  assert(facade.readSelectedImage().content == imported.content);
  cancelFile = false; fileName = "fc03-red.jpg";
  activate("Replace selected image...");
  const auto replaced = facade.readSelectedImage();
  assert(replaced.content.resource_id != imported.content.resource_id);
  assert(replaced.objectId == imported.objectId);
  assert(replaced.content.width == imported.content.width && replaced.transform == imported.transform);
  assert(facade.undo(600, facade.readRuntimeState().identity.runtimeGeneration).state == runtime::ProductControlState::kQueued);
  assert(host.composition().ownerPump());
  assert(facade.readSelectedImage().content == imported.content);
  // Observe real derived pixels while canonical remains unchanged.
  assert(host.composition().setPlatformPresentationDeferred(true));
  const auto renderPixels = [&] {
    runtime::RuntimeFrameOpportunity frame;
    assert(host.composition().serviceFrameOpportunity(true, frame));
    std::vector<std::uint8_t> rgba(800 * 600 * 4);
    assert(host.composition().readCanonicalRgba(rgba));
    assert(frame.canonicalWork);
    assert(host.composition().receivePlatformPresented({*frame.canonicalWork, render::PresentOutcome::kPresented,
        render::PresentEvidenceKind::kPlatformQualified, {}}));
    return rgba;
  };
  const auto originalPixels = renderPixels();
  const auto before = host.composition().readback().semanticGeneration;
  activate("Crop selected image");
  assert(facade.readImageCrop().active);
  assert(host.composition().readback().semanticGeneration == before);
  assert(host.registerSource(87));
  std::uint64_t pointerSequence = 0;
  const auto pointer = [&](input::PointerPhase phase, float x, float y) {
    input::PlatformPointerBatch batch;
    batch.samples.push_back({87, 1, ++pointerSequence, pointerSequence * 1000000,
        x, y, 0, 0, 0, {}, {}, input::SampleProvenance::kConfirmedCurrent, phase});
    assert(host.submit(batch));
  };
  // Crop handles edit the temporary source/display box, never Transform.
  pointer(input::PointerPhase::kDown, 588, 238);
  pointer(input::PointerPhase::kMove, 550, 200);
  pointer(input::PointerPhase::kUp, 550, 200);
  assert(facade.readImageCrop().content.width == 100);
  assert(facade.readImageCrop().content.height == 100);
  assert(host.composition().readback().semanticGeneration == before);
  assert(facade.readSelectedImage().transform == imported.transform);
  const auto token = facade.readImageCrop().cropToken;
  assert(facade.updateImageCrop({token, {.25, 0, .5, 1}, 200, 200}).state == runtime::ImageCommandState::kApplied);
  assert(renderPixels() != originalPixels);
  assert(facade.readSelectedImage().content == imported.content);
  activate("Crop Done");
  assert(!facade.readImageCrop().active);
  assert(facade.readSelectedImage().content.source_rect == semantic::NormalizedRect(.25, 0, .5, 1));
  assert(facade.undo(601, facade.readRuntimeState().identity.runtimeGeneration).state == runtime::ProductControlState::kQueued);
  assert(host.composition().ownerPump());
  assert(facade.readSelectedImage().content == imported.content);
  activate("Crop selected image");
  assert(facade.updateImageCrop({facade.readImageCrop().cropToken, {.5, 0, .5, 1}, 100, 100}).state == runtime::ImageCommandState::kApplied);
  activate("Crop Cancel");
  assert(!facade.readImageCrop().active);
  assert(renderPixels() == originalPixels);

  // A Crop preview must reveal the full source beyond its fixed trim frame,
  // dim only that outside region, and let that visible area pan the source.
  runtime::ImageImportRequest stripe;
  std::ifstream stripeFile(std::filesystem::path(FC03_FIXTURE_ROOT).parent_path().parent_path() /
      "fixtures/g47/image-stripes.png", std::ios::binary);
  stripe.bytes = {std::istreambuf_iterator<char>(stripeFile), {}};
  stripe.mimeType = "image/png";
  stripe.width = stripe.height = 100;
  stripe.sourceRect = semantic::NormalizedRect{.5, 0, .5, 1};
  stripe.transform.tx = 250; stripe.transform.ty = 350;
  const auto stripeInserted = facade.importImage(stripe);
  assert(stripeInserted.state == runtime::ImageCommandState::kApplied);
  const auto stripeBefore = renderPixels();
  const auto rgbaAt = [](const auto& pixels, int x, int y) {
    const auto offset = static_cast<std::size_t>((y*800+x)*4);
    return std::array<std::uint8_t,4>{pixels[offset],pixels[offset+1],pixels[offset+2],pixels[offset+3]};
  };
  const std::array<std::uint8_t,4> white{255,255,255,255}, blue{0,0,255,255};
  assert(rgbaAt(stripeBefore,180,400) == white);
  const auto generationBeforeCrop = host.composition().readback().semanticGeneration;
  auto stripeCrop = facade.beginImageCrop({stripeInserted.objectId});
  assert(stripeCrop.state == runtime::ImageCommandState::kApplied);
  const auto stripePreview = renderPixels();
  const auto outside = rgbaAt(stripePreview,180,400);
  assert(outside[0] > 70 && outside[0] < 200 && outside[1] == 0 && outside[2] == 0 && outside[3] == 255);
  assert(rgbaAt(stripePreview,300,400) == blue);
  assert(rgbaAt(stripePreview,140,400) == white);
  assert(facade.readSelectedImage().content == stripeInserted.content);
  assert(host.composition().readback().semanticGeneration == generationBeforeCrop);
  pointer(input::PointerPhase::kDown,180,400); // visible source outside the trim frame
  pointer(input::PointerPhase::kMove,230,400);
  pointer(input::PointerPhase::kUp,230,400);
  assert(facade.readImageCrop().content.source_rect->x == .25);
  assert(host.composition().readback().semanticGeneration == generationBeforeCrop);
  assert(facade.cancelImageCrop(stripeCrop.cropToken).state == runtime::ImageCommandState::kApplied);
  assert(renderPixels() == stripeBefore);
  stripeCrop = facade.beginImageCrop({stripeInserted.objectId});
  assert(stripeCrop.state == runtime::ImageCommandState::kApplied);
  assert(facade.updateImageCrop({stripeCrop.cropToken,{0,0,.25,1},100,100}).state == runtime::ImageCommandState::kApplied);
  const auto narrowPreview = renderPixels();
  assert(rgbaAt(narrowPreview,260,400) == white); // Fit letterbox remains identical to Done
  assert(facade.cancelImageCrop(stripeCrop.cropToken).state == runtime::ImageCommandState::kApplied);
  stripeCrop = facade.beginImageCrop({stripeInserted.objectId});
  assert(stripeCrop.state == runtime::ImageCommandState::kApplied);
  assert(facade.updateImageCrop({stripeCrop.cropToken,{.25,0,.5,1},100,100}).state == runtime::ImageCommandState::kApplied);
  // x=205 is inside the full-source backdrop and outside the selected
  // image's left Connector affordance (center x=222, radius 4).
  const auto beforeDone = rgbaAt(renderPixels(),205,400);
  assert(beforeDone[0] > 70 && beforeDone[0] < 200 && beforeDone[1] == 0 && beforeDone[2] == 0 && beforeDone[3] == 255);
  assert(facade.commitImageCrop(stripeCrop.cropToken).state == runtime::ImageCommandState::kApplied);
  const auto committedStripe = renderPixels();
  assert(rgbaAt(committedStripe,205,400) == white); // editing backdrop disappears
  assert(facade.undo(602,facade.readRuntimeState().identity.runtimeGeneration).state == runtime::ProductControlState::kQueued);
  assert(host.composition().ownerPump());
  assert(renderPixels() == stripeBefore);
  ImGui::DestroyContext();
}
