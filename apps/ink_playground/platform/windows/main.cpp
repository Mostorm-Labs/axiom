#include "ink_playground_host.hpp"
#include "platform_brush_baseline_observation.hpp"
#include "arc/arc.hpp"
#include "windows_pointer_utils.hpp"
#include "windows_smoke_evidence.hpp"
#include "canvas/render/canonical_handoff.hpp"
#include "canvas/ink/arc_runtime_sinks.hpp"
#include "windows_skia_preview_surface_provider.hpp"
#if defined(CANVAS_RENDER_HAS_SKIA)
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <sstream>
#include <span>
#include <unordered_map>
#include <vector>

namespace {
using canvas::ink_playground::InkPlaygroundHost;
class WindowsArcRuntimeSinks;

struct State { HWND window = nullptr; std::unique_ptr<InkPlaygroundHost> host;
  std::unique_ptr<arc::Bridge> previewBridge; std::uint64_t previewGeneration = 1;
  std::uint64_t stroke = 0;
  // GetMessageTime() has millisecond resolution and several mouse messages
  // can arrive in one tick. BrushSession requires strictly increasing
  // confirmed sample sequences, so keep an independent process-local clock.
  std::uint64_t mouseSampleSequence = 0;
  std::uint64_t deviceId = 0;
  std::unordered_map<std::uint64_t, canvas::input::PointerKey> activeKeys;
  std::unordered_map<std::uint64_t, std::uint64_t> pointerStrokes;
  std::vector<canvas::ink_playground::windows_input::PointerEvidenceSample> trace;
  std::size_t maxConcurrentPointers = 0;
  std::uint64_t cpuCopyCount = 0;
  std::uint64_t presentCount = 0;
  bool runtimePreviewVisible = true;
  std::unique_ptr<WindowsArcRuntimeSinks> runtimeSinks;
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::SkiaRenderer> canonicalRenderer;
  canvas::render::RasterSkiaSurfaceProvider* canonicalProvider = nullptr;
  canvas::ink_playground::WindowsSkiaPreviewSurfaceProvider* previewProvider = nullptr;
  std::vector<std::uint8_t> canonicalBgra;
  std::uint64_t canonicalRasterization = 0;
#endif
};
struct NativePointerSample final {
  std::uint64_t pointer_id = 0;
  std::uint64_t sample_sequence = 0;
  std::uint64_t timestamp_us = 0;
  float x = 0.0F;
  float y = 0.0F;
  float pressure = 0.0F;
  std::uint32_t phase = ARC_POINTER_PHASE_MOVE;
  std::uint32_t provenance = ARC_SAMPLE_CONFIRMED_CURRENT;
};
State* state(HWND window) { return reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA)); }
void persistEvidence(const State& value);

bool attachPreviewTarget(State& value, std::uint32_t width,
                         std::uint32_t height) noexcept {
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (value.previewBridge == nullptr || value.previewProvider == nullptr ||
      width == 0U || height == 0U) return false;
  value.previewGeneration = value.previewProvider->generation();
  arc_preview_target_v0 target{};
  target.struct_size = sizeof(target);
  target.abi_version = ARC_ABI_VERSION;
  target.platform_kind = ARC_PLATFORM_WINDOWS;
  target.target_id = 1U;
  target.target_generation = value.previewGeneration;
  target.width_pixels = width;
  target.height_pixels = height;
  target.device_pixel_ratio = 1.0F;
  target.opaque_platform_handle = reinterpret_cast<std::uint64_t>(value.previewProvider);
  return value.previewBridge->Attach(target) == arc::Status::kOk;
#else
  (void)value;
  (void)width;
  (void)height;
  return false;
#endif
}

// Windows is an ARC realization only. Runtime owns preview/session semantics;
// this adapter translates the typed runtime seam into the ARC ABI and keeps
// platform preview state out of the canonical document/render path.
class WindowsArcRuntimeSinks final : public canvas::ink::ArcPreviewSink,
                                     public canvas::ink::CanonicalVisibilitySink {
 public:
  explicit WindowsArcRuntimeSinks(State& state) : state_(state) {}
  canvas::ink::PreviewSubmitResult begin(
      const canvas::ink::PreviewIdentity& identity,
      const canvas::ink::BrushPreviewDelta&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::PreviewSubmitResult::kCanonicalOnly;
    arc_preview_begin_v0 begin{};
    begin.struct_size = sizeof(begin); begin.abi_version = ARC_ABI_VERSION;
    begin.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; begin.stroke_id = identity.session;
    begin.view_id = 1U; begin.viewport_revision = identity.sessionGeneration;
    begin.target_generation = state_.previewGeneration;
    begin.brush.struct_size = sizeof(begin.brush); begin.brush.abi_version = ARC_ABI_VERSION;
    return state_.previewBridge->Begin(begin) == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::PreviewSubmitResult update(
      const canvas::ink::PreviewIdentity& identity,
      const canvas::ink::BrushPreviewDelta&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::PreviewSubmitResult::kCanonicalOnly;
    arc_preview_update_v0 update{};
    update.struct_size = sizeof(update); update.abi_version = ARC_ABI_VERSION;
    update.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; update.stroke_id = identity.session;
    update.preview_revision = ++revisions_[identity.session];
    update.view_id = 1U;
    update.viewport_revision = identity.sessionGeneration;
    update.target_generation = state_.previewGeneration;
    // The ARC protocol is lifecycle-only on Windows. Runtime's Skia preview
    // provider owns and renders BrushPreviewDelta.outline.
    return state_.previewBridge->Push(update) == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::PreviewSubmitResult cancel(
      const canvas::ink::PreviewIdentity& identity) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::PreviewSubmitResult::kCanonicalOnly;
    arc_preview_cancel_v0 cancel{sizeof(cancel), ARC_ABI_VERSION, identity.session,
                                 state_.previewGeneration, 1U, 0U};
    return state_.previewBridge->Cancel(cancel) == arc::Status::kOk
        ? canvas::ink::PreviewSubmitResult::kAccepted
        : canvas::ink::PreviewSubmitResult::kCanonicalOnly;
  }
  canvas::ink::HandoffResult canonicalCommitted(
      const canvas::ink::CanonicalHandoffIdentity& identity,
      const canvas::semantic::CanonicalCommitRecord&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::HandoffResult::kIgnored;
    const auto revision = revisions_[identity.session];
    if (revision == 0U) return canvas::ink::HandoffResult::kRejected;
    arc_preview_seal_v0 seal{sizeof(seal), ARC_ABI_VERSION, identity.session,
                             revision, state_.previewGeneration};
    if (state_.previewBridge->SealInput(seal) != arc::Status::kOk) {
      return canvas::ink::HandoffResult::kIgnored;
    }
    arc_canonical_commit_v0 commit{sizeof(commit), ARC_ABI_VERSION, identity.session,
                                   revision, identity.commit.ordinal.value(),
                                   state_.previewGeneration, {identity.commit.runtime_epoch.value(),
                                                              identity.commit.ordinal.value()}};
    return state_.previewBridge->CanonicalCommitted(commit) == arc::Status::kOk
        ? canvas::ink::HandoffResult::kAccepted
        : canvas::ink::HandoffResult::kIgnored;
  }
  canvas::ink::HandoffResult canonicalVisible(
      const canvas::ink::CanonicalHandoffIdentity& identity,
      const canvas::render::FrameState&) noexcept override {
    if (state_.previewBridge == nullptr) return canvas::ink::HandoffResult::kIgnored;
    arc_canonical_visible_v0 visible{};
    visible.struct_size = sizeof(visible); visible.abi_version = ARC_ABI_VERSION;
    visible.stroke_id = identity.session; visible.document_revision = identity.commit.ordinal.value();
    visible.target_generation = identity.surfaceGeneration.value();
    visible.handoff_token = {identity.commit.runtime_epoch.value(), identity.commit.ordinal.value()};
    visible.receipt.struct_size = sizeof(visible.receipt);
    visible.receipt.abi_version = ARC_ABI_VERSION;
    visible.receipt.evidence = ARC_EVIDENCE_DETERMINISTIC_ORACLE;
    visible.receipt.status = ARC_STATUS_OK;
    visible.receipt.target_generation = identity.surfaceGeneration.value();
    visible.receipt.presentation_id = identity.commit.ordinal.value();
    return state_.previewBridge->CanonicalVisible(visible) == arc::Status::kOk
        ? canvas::ink::HandoffResult::kAccepted
        : canvas::ink::HandoffResult::kIgnored;
  }
 private:
  State& state_;
  std::unordered_map<std::uint64_t, std::uint64_t> revisions_;
};

bool renderCanonical(State& value) {
#if defined(CANVAS_RENDER_HAS_SKIA)
  return value.canonicalRenderer != nullptr &&
         // While a mouse/pointer is down, the canonical redraw is only a
         // backing-surface update. The matching CanonicalVisible handoff must
         // happen after release, otherwise WM_PAINT clears the live Arc
         // preview before the user can see it.
         value.host->presentCanonicalFrame(value.host->canonicalFrameCount() + 1U, 0.0,
                                           value.activeKeys.empty());
#else
  return false;
#endif
}

bool submitMouseSample(HWND window, State& value, UINT message, WPARAM wParam, LPARAM lParam) {
  constexpr UINT kMousePointerId = 1U;
  const auto timestampMs = static_cast<std::uint64_t>(GetMessageTime());
  bool begin = false;
  bool end = false;
  std::uint32_t phase = ARC_POINTER_PHASE_MOVE;
  if (message == WM_LBUTTONDOWN) {
    begin = true;
    phase = ARC_POINTER_PHASE_DOWN;
  } else if (message == WM_LBUTTONUP) {
    end = value.activeKeys.contains(kMousePointerId);
    phase = ARC_POINTER_PHASE_UP;
  } else if ((wParam & MK_LBUTTON) == 0U || !value.activeKeys.contains(kMousePointerId)) {
    return false;
  }
  if (begin) {
    ++value.stroke;
    SetCapture(window);
  }

  const auto x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
  const auto y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
  NativePointerSample sample{};
  sample.pointer_id = kMousePointerId;
  sample.sample_sequence = ++value.mouseSampleSequence;
  sample.timestamp_us = timestampMs * 1000U;
  sample.x = x;
  sample.y = y;
  sample.pressure = 0.5F;
  sample.phase = phase;
  sample.provenance = ARC_SAMPLE_CONFIRMED_CURRENT;
  canvas::input::PlatformPointerBatch common;
  common.samples.push_back({1U, kMousePointerId, sample.sample_sequence,
      sample.timestamp_us * 1000U, sample.x, sample.y, sample.pressure, 0.0F, 0.0F,
      {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
      phase == ARC_POINTER_PHASE_DOWN ? canvas::input::PointerPhase::kDown
      : phase == ARC_POINTER_PHASE_UP ? canvas::input::PointerPhase::kUp
                                      : canvas::input::PointerPhase::kMove});
  if (!value.host->acceptPlatformBatch(common, sample.timestamp_us * 1000U)) return false;
  if (begin) {
    const auto key = value.host->platformKey(1U, kMousePointerId);
    if (!key) return false;
    value.activeKeys[kMousePointerId] = *key;
    value.pointerStrokes[kMousePointerId] = value.host->platformStrokeId(*key).value_or(value.stroke);
    value.maxConcurrentPointers = (std::max)(value.maxConcurrentPointers, value.activeKeys.size());
  }
  value.deviceId = kMousePointerId;
  const auto key = value.activeKeys.at(kMousePointerId);
  value.trace.push_back({key.source, kMousePointerId, key.generation, timestampMs, "mouse",
                          phase == ARC_POINTER_PHASE_DOWN ? "down" :
                          (phase == ARC_POINTER_PHASE_UP ? "up" : "move"),
                          x, y, sample.pressure, 1U, 0.0F, 0.0F});
  if (end) persistEvidence(value);
  if (end) {
    value.activeKeys.erase(kMousePointerId);
    value.pointerStrokes.erase(kMousePointerId);
    if (GetCapture() == window) ReleaseCapture();
  }
  InvalidateRect(window, nullptr, FALSE);
  return true;
}

void cancelPointer(State& value, std::uint64_t pointerId, std::uint64_t timestampMs) {
  const auto keyEntry = value.activeKeys.find(pointerId);
  if (keyEntry == value.activeKeys.end()) return;
  const auto key = keyEntry->second;
  const auto strokeEntry = value.pointerStrokes.find(pointerId);
  if (strokeEntry != value.pointerStrokes.end()) {
    (void)value.host->cancelBrushSession(pointerId);
  }
  value.trace.push_back({key.source, static_cast<std::uint32_t>(pointerId), key.generation,
                         timestampMs, "unknown", "cancel", 0.0F, 0.0F, 0.0F, 0U,
                         0.0F, 0.0F});
  value.activeKeys.erase(pointerId);
  value.pointerStrokes.erase(pointerId);
  persistEvidence(value);
}

void cancelAllPointers(State& value, std::uint64_t timestampMs) {
  for (const auto& [pointerId, key] : value.activeKeys) {
    value.trace.push_back({key.source, static_cast<std::uint32_t>(pointerId), key.generation,
                           timestampMs, "unknown", "cancel", 0.0F, 0.0F, 0.0F, 0U,
                           0.0F, 0.0F});
  }
  value.host->cancelAllPointers();
  value.activeKeys.clear();
  value.pointerStrokes.clear();
  if (GetCapture() == value.window) ReleaseCapture();
  persistEvidence(value);
}

void persistEvidence(const State& value) {
  wchar_t buffer[32768]{};
  const DWORD length = GetEnvironmentVariableW(L"AXIOM_INK_EVIDENCE_DIR", buffer,
                                                static_cast<DWORD>(std::size(buffer)));
  if (length == 0 || length >= std::size(buffer)) return;
  const std::filesystem::path directory(buffer);
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) return;
  std::ofstream traceFile(directory / "pointer-trace.json", std::ios::binary | std::ios::trunc);
  traceFile << canvas::ink_playground::windows_input::serializePointerTrace(value.trace,
                                                                            value.deviceId);
  const auto& hud = value.host->hud();
  std::ofstream hudFile(directory / "hud-telemetry.json", std::ios::binary | std::ios::trunc);
  hudFile << "{\n  \"schema_version\": \"0.1\",\n"
          << "  \"sample_hz\": " << hud.sampleHz << ",\n"
          << "  \"batch_size\": " << hud.batch << ",\n"
          << "  \"queue_age_ms\": " << hud.queueAgeMs << ",\n"
          << "  \"ink_processing_ms\": " << hud.inkMs << ",\n"
          << "  \"preview_revision\": " << hud.previewRevision << ",\n"
          << "  \"prediction_depth\": " << hud.predictionDepth << ",\n"
          << "  \"presentation_evidence_kind\": \"" << hud.presentEvidenceKind << "\",\n"
          << "  \"pending_handoff_count\": " << hud.pendingHandoffCount << ",\n"
          << "  \"frame_time_ms\": " << hud.frameMs << ",\n"
          << "  \"active_pointer_count\": " << value.activeKeys.size() << ",\n"
          << "  \"max_concurrent_pointers\": " << value.maxConcurrentPointers << ",\n"
          << "  \"multi_contact_policy\": \""
          << canvas::ink_playground::windows_input::multiContactPolicyNameUtf8(
                 value.host->multiContactPolicy()) << "\",\n"
          << "  \"viewport_scale\": " << value.host->viewportGesture().scale << ",\n"
          << "  \"viewport_translation_x\": "
          << value.host->viewportGesture().translationX << ",\n"
          << "  \"viewport_translation_y\": "
          << value.host->viewportGesture().translationY << ",\n"
          << "  \"render_path\": {\n"
          << "    \"renderer\": \"Skia raster\",\n"
          << "    \"surface_type\": \"CPU raster buffer to GDI\",\n"
#if defined(CANVAS_RENDER_HAS_SKIA)
          << "    \"submission_count\": "
          << (value.canonicalRenderer ? value.canonicalRenderer->submissionCount() : 0) << ",\n"
          << "    \"readback_count\": "
          << (value.canonicalProvider ? value.canonicalProvider->readbackCount() : 0) << ",\n"
#else
          << "    \"submission_count\": 0,\n    \"readback_count\": 0,\n"
#endif
          << "    \"cpu_copy_count\": " << value.cpuCopyCount << ",\n"
          << "    \"present_count\": " << value.presentCount << ",\n"
          << "    \"viewport_transform_applied\": \"true\"\n  }\n}\n";
  std::ofstream baselineFile(directory / "observation.json", std::ios::binary | std::ios::trunc);
  wchar_t modulePath[MAX_PATH]{};
  const DWORD moduleLength = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  std::error_code artifactError;
  const auto artifact = moduleLength == 0 ? std::string{} :
      std::filesystem::path(modulePath).filename().string() + ":" +
      std::to_string(std::filesystem::file_size(modulePath, artifactError));
#if defined(CANVAS_RENDER_HAS_SKIA)
  const auto submissions = value.canonicalRenderer ? value.canonicalRenderer->submissionCount() : 0;
  const auto readbacks = value.canonicalProvider ? value.canonicalProvider->readbackCount() : 0;
  const auto copies = value.cpuCopyCount +
      (value.canonicalProvider ? value.canonicalProvider->cpuCopyCount() : 0);
#else
  const std::uint64_t submissions = 0;
  const std::uint64_t readbacks = 0;
  const auto copies = value.cpuCopyCount;
#endif
  canvas::ink_playground::BaselineRenderPath renderPath{
      "WM_POINTER history→PlatformPointerBatch", "C++ InkPlaygroundHost", "Skia raster",
      "CPU raster buffer→BGRA copy→GDI", submissions, readbacks, copies,
      value.presentCount, "true"};
  renderPath.canonicalProviderIdentity = "windows-canonical-raster";
  renderPath.previewProviderIdentity = "windows-skia-arc-preview";
  baselineFile << canvas::ink_playground::platformBrushBaselineObservationJson(
      "windows", "PHYSICAL", artifactError ? std::string{} : artifact, *value.host,
      renderPath);
}

void paint(HWND window, State& value) {
  PAINTSTRUCT ps{}; HDC dc = BeginPaint(window, &ps); RECT rect{}; GetClientRect(window, &rect);
  HDC bufferDc = CreateCompatibleDC(dc);
  HBITMAP bitmap = CreateCompatibleBitmap(dc, rect.right - rect.left, rect.bottom - rect.top);
  HGDIOBJ oldBitmap = SelectObject(bufferDc, bitmap);
  FillRect(bufferDc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  const bool previewPresented = value.host->presentBrushPreview();
  bool canonicalPresented = false;
#if defined(CANVAS_RENDER_HAS_SKIA)
  canonicalPresented = renderCanonical(value);
  const auto width = static_cast<std::uint32_t>(rect.right);
  const auto height = static_cast<std::uint32_t>(rect.bottom);
   // Surface dimensions and generations are owned by InkPlaygroundHost::resizeSurface.
  if (value.canonicalBgra.size() != static_cast<std::size_t>(width) * height * 4U) {
    value.canonicalBgra.resize(static_cast<std::size_t>(width) * height * 4U);
  }
  if (value.canonicalProvider->readbackRgba(value.canonicalBgra).code ==
      canvas::render::BackendSubmissionCode::kAccepted) {
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(width);
    info.bmiHeader.biHeight = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    const auto rasterization = value.canonicalRenderer->rasterizationCount();
    if (value.canonicalRasterization != rasterization) {
      ++value.cpuCopyCount;
      for (std::size_t i = 0; i + 3 < value.canonicalBgra.size(); i += 4)
        std::swap(value.canonicalBgra[i], value.canonicalBgra[i + 2]);
      value.canonicalRasterization = rasterization;
    }
    SetDIBitsToDevice(bufferDc, 0, 0, width, height, 0, 0, 0,
                      height, value.canonicalBgra.data(), &info,
                      DIB_RGB_COLORS);
    ++value.presentCount;
  }
#endif
  HPEN pen = CreatePen(PS_SOLID, 3, RGB(26, 91, 255)); HGDIOBJ oldPen = SelectObject(bufferDc, pen);
  // Some Windows compositor configurations accept the layered Arc overlay
  // update but do not expose it above a redirected top-level window. Keep a
  // presentation-only Amber fallback in the owner surface, using the same
  // Runtime preview points. It never enters canonical state or handoff.
  if (value.host->previewActive()) {
    const auto preview = value.host->brushPreviewOutline();
    if (preview.size() >= 2U) {
      std::vector<POINT> polygon;
      polygon.reserve(preview.size());
      for (const auto& point : preview) {
        polygon.push_back({static_cast<LONG>(point.x), static_cast<LONG>(point.y)});
      }
      HBRUSH amber = CreateSolidBrush(RGB(255, 170, 0));
      HGDIOBJ oldAmber = SelectObject(bufferDc, amber);
      HPEN outline = CreatePen(PS_SOLID, 1, RGB(255, 170, 0));
      HGDIOBJ oldOutline = SelectObject(bufferDc, outline);
      Polygon(bufferDc, polygon.data(), static_cast<int>(polygon.size()));
      SelectObject(bufferDc, oldOutline);
      SelectObject(bufferDc, oldAmber);
      DeleteObject(outline);
      DeleteObject(amber);
    }
  }
  SetBkMode(bufferDc, TRANSPARENT);
  SetTextColor(bufferDc, RGB(30, 30, 30));
  const auto& hud = value.host->hud();
  std::wstringstream status;
  const wchar_t* qualificationState = value.host->previewActive()
                                          ? L"PREVIEW_OR_OVERLAP"
                                          : L"CANONICAL_ONLY";
  status << L"Axiom Ink Playground | HWND ready | WM_POINTER enabled | samples: "
         << value.trace.size() << L" | batch: " << hud.batch
         << L" | pointers: " << value.activeKeys.size()
         << L" | mode: " << canvas::ink_playground::windows_input::multiContactPolicyName(
                                value.host->multiContactPolicy())
         << L" | viewport: " << value.host->viewportGesture().scale << L"x @ "
         << value.host->viewportGesture().translationX << L","
         << value.host->viewportGesture().translationY
         << L" | preview_present: " << value.host->previewPresentCount()
         << L" | state: " << qualificationState
         << L" | Arc preview: Skia layered"
         << L" | runtime preview: " << (value.runtimePreviewVisible ? L"ON" : L"OFF")
         << L" | SPACE = preview | P = pointer mode";
  const auto text = status.str();
  TextOutW(bufferDc, 16, 16, text.c_str(), static_cast<int>(text.size()));
  // Optional presentation-only mirror. It is never used for canonical
  // handoff eligibility and is off by default so Arc evidence cannot be
  // satisfied by this debug layer.
  // Preview pixels are owned by the independent ARC overlay/provider. The
  // canonical HWND is never a second preview raster path.
  BitBlt(dc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, bufferDc, 0, 0, SRCCOPY);
  (void)previewPresented;
  (void)canonicalPresented;
  SelectObject(bufferDc, oldPen); DeleteObject(pen);
  SelectObject(bufferDc, oldBitmap); DeleteObject(bitmap); DeleteDC(bufferDc);
  EndPaint(window, &ps);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* value = state(window);
  if (message == WM_NCCREATE) {
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(
        static_cast<CREATESTRUCTW*>(reinterpret_cast<void*>(lParam))->lpCreateParams));
    return TRUE;
  }
  if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_MOUSEMOVE) {
    if (value != nullptr) {
      (void)submitMouseSample(window, *value, message, wParam, lParam);
    }
    return 0;
  }
  if (message == WM_SIZE && value != nullptr) {
#if defined(CANVAS_RENDER_HAS_SKIA)
    const auto width = static_cast<std::uint32_t>(LOWORD(lParam));
    const auto height = static_cast<std::uint32_t>(HIWORD(lParam));
    if (width != 0U && height != 0U && value->previewProvider != nullptr &&
        value->host->resizeSurface(width, height) &&
        attachPreviewTarget(*value, width, height)) {
      InvalidateRect(window, nullptr, FALSE);
    }
#else
    (void)lParam;
#endif
    return 0;
  }
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP) {
    if (value == nullptr) return 0;
    const UINT pointerId = GET_POINTERID_WPARAM(wParam);
    POINTER_INFO info{};
    if (!GetPointerInfo(pointerId, &info)) return 0;
    if (info.pointerType == PT_MOUSE) return 0;
    const auto phase = message == WM_POINTERDOWN ? ARC_POINTER_PHASE_DOWN :
        (message == WM_POINTERUP ? ARC_POINTER_PHASE_UP : ARC_POINTER_PHASE_MOVE);
    bool begin = false;
    bool end = false;
    if (phase == ARC_POINTER_PHASE_DOWN) {
      begin = true;
    } else if (phase == ARC_POINTER_PHASE_UP) {
      end = value->activeKeys.contains(pointerId);
    } else if (!value->activeKeys.contains(pointerId)) {
      return 0;
    }
    if (begin) {
      ++value->stroke;
      value->pointerStrokes[pointerId] = value->stroke;
    }
    std::vector<POINTER_INFO> pointerHistory;
    UINT32 pointerHistoryCount = 0;
    if (!GetPointerInfoHistory(pointerId, &pointerHistoryCount, nullptr) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) return 0;
    if (pointerHistoryCount != 0U) {
      pointerHistory.resize(pointerHistoryCount);
      if (!GetPointerInfoHistory(pointerId, &pointerHistoryCount, pointerHistory.data())) return 0;
      pointerHistory.resize(pointerHistoryCount);
    }
    if (pointerHistory.empty()) pointerHistory.push_back(info);
    if (phase == ARC_POINTER_PHASE_DOWN) {
      pointerHistory.assign(1U, info);
    } else {
      pointerHistory = canvas::ink_playground::windows_input::normalizePointerHistory(
          pointerHistory, false);
    }
    if (pointerHistory.empty()) pointerHistory.push_back(info);
    std::vector<POINTER_PEN_INFO> penHistory;
    if (info.pointerType == PT_PEN) {
      UINT32 penHistoryCount = 0;
      if (!GetPointerPenInfoHistory(pointerId, &penHistoryCount, nullptr) &&
          GetLastError() != ERROR_INSUFFICIENT_BUFFER) return 0;
      if (penHistoryCount != 0U) {
        penHistory.resize(penHistoryCount);
        if (!GetPointerPenInfoHistory(pointerId, &penHistoryCount, penHistory.data())) return 0;
        penHistory.resize(penHistoryCount);
        std::reverse(penHistory.begin(), penHistory.end());
        if (penHistory.size() > pointerHistory.size()) {
          penHistory.erase(penHistory.begin(),
                           penHistory.end() - static_cast<std::ptrdiff_t>(pointerHistory.size()));
        }
      }
    }
    std::vector<NativePointerSample> samples;
    samples.reserve(pointerHistory.size());
    float contactWidth = 0.0F;
    float contactHeight = 0.0F;
    if (info.pointerType == PT_TOUCH) {
      POINTER_TOUCH_INFO touchInfo{};
      if (GetPointerTouchInfo(pointerId, &touchInfo) != FALSE) {
        contactWidth = static_cast<float>(
            std::abs(touchInfo.rcContact.right - touchInfo.rcContact.left));
        contactHeight = static_cast<float>(
            std::abs(touchInfo.rcContact.bottom - touchInfo.rcContact.top));
      }
    }
    for (std::size_t index = 0; index < pointerHistory.size(); ++index) {
      const auto& history = pointerHistory[index];
      const auto historyPhase = index == 0U && phase == ARC_POINTER_PHASE_DOWN
                                    ? ARC_POINTER_PHASE_DOWN
                                    : (index + 1U == pointerHistory.size() && phase == ARC_POINTER_PHASE_UP
                                           ? ARC_POINTER_PHASE_UP
                                           : ARC_POINTER_PHASE_MOVE);
      float pressure = 0.5F;
      if (index < penHistory.size()) pressure = static_cast<float>(penHistory[index].pressure) / 1024.0F;
      NativePointerSample sample{};
      sample.pointer_id = pointerId;
      sample.sample_sequence = (static_cast<std::uint64_t>(history.dwTime) << 16U) | index;
      sample.timestamp_us = static_cast<std::uint64_t>(history.dwTime) * 1000U + index;
      POINT clientPoint = history.ptPixelLocation;
      if (ScreenToClient(window, &clientPoint) == FALSE) return 0;
      sample.x = static_cast<float>(clientPoint.x);
      sample.y = static_cast<float>(clientPoint.y);
      sample.pressure = std::clamp(pressure, 0.0F, 1.0F);
      sample.phase = historyPhase;
      sample.provenance = ARC_SAMPLE_CONFIRMED_CURRENT;
      samples.push_back(sample);
    }
    value->deviceId = canvas::ink_playground::windows_input::functionalDeviceId(
        info.sourceDevice, static_cast<std::uint64_t>(pointerId));
    canvas::input::PlatformPointerBatch common;
    common.samples.reserve(samples.size());
    for (const auto& sample : samples) {
      const auto historyPhase = sample.phase == ARC_POINTER_PHASE_DOWN
          ? canvas::input::PointerPhase::kDown
          : sample.phase == ARC_POINTER_PHASE_UP ? canvas::input::PointerPhase::kUp
          : canvas::input::PointerPhase::kMove;
      common.samples.push_back({value->deviceId, pointerId, sample.sample_sequence,
          sample.timestamp_us * 1000U, sample.x, sample.y, sample.pressure, 0.0F, 0.0F,
          {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent, historyPhase});
    }
    if (!value->host->acceptPlatformBatch(common,
        static_cast<std::uint64_t>(GetMessageTime()) * 1'000'000U)) return 0;
    if (begin) {
      const auto key = value->host->platformKey(value->deviceId, pointerId);
      if (!key) return 0;
      value->activeKeys[pointerId] = *key;
      value->pointerStrokes[pointerId] = value->host->platformStrokeId(*key).value_or(value->stroke);
    }
    // Register a new touch/pen key before recording its coalesced history.
    // WM_POINTERDOWN reaches this adapter before the host has exposed the
    // keyed session, so reading activeKeys.at(pointerId) in the history loop
    // would throw std::out_of_range and terminate the process on the first
    // real touch stroke.
    const auto active = value->activeKeys.find(pointerId);
    if (active == value->activeKeys.end()) return 0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
      const auto& sample = samples[index];
      const auto& history = pointerHistory[index];
      value->trace.push_back({active->second.source, pointerId, active->second.generation,
                              history.dwTime,
                              info.pointerType == PT_PEN ? "pen" :
                              (info.pointerType == PT_TOUCH ? "touch" : "mouse"),
                              sample.phase == ARC_POINTER_PHASE_DOWN ? "down" :
                              (sample.phase == ARC_POINTER_PHASE_UP ? "up" : "move"),
                              sample.x, sample.y, sample.pressure, pointerHistory.size(),
                              contactWidth, contactHeight});
    }
    // Runtime owns viewport arbitration and typed ARC preview publication.
    if (end) persistEvidence(*value);
    if (end) {
      value->activeKeys.erase(pointerId);
      value->pointerStrokes.erase(pointerId);
    }
    InvalidateRect(window, nullptr, FALSE); return 0;
  }
  if (message == WM_POINTERCAPTURECHANGED) {
    if (value != nullptr) {
      cancelPointer(*value, GET_POINTERID_WPARAM(wParam),
                    static_cast<std::uint64_t>(GetMessageTime()));
      InvalidateRect(window, nullptr, FALSE);
    }
    return 0;
  }
  if (canvas::ink_playground::windows_input::cancelsAllActivePointers(message)) {
    if (value != nullptr && !value->activeKeys.empty()) {
      cancelAllPointers(*value, static_cast<std::uint64_t>(GetMessageTime()));
      InvalidateRect(window, nullptr, FALSE);
    }
    if (message == WM_DESTROY) {
      PostQuitMessage(0);
      return 0;
    }
    return 0;
  }
  if (message == WM_PAINT) { if (value != nullptr) paint(window, *value); return 0; }
  if (message == WM_KEYDOWN && wParam == VK_SPACE) {
    if (value != nullptr && (lParam & (1LL << 30)) == 0) {
      value->runtimePreviewVisible = !value->runtimePreviewVisible;
      value->host->setRuntimePreviewVisible(value->runtimePreviewVisible);
      InvalidateRect(window, nullptr, FALSE);
    }
    return 0;
  }
  if (message == WM_KEYDOWN && wParam == L'P') {
    if (value != nullptr && (lParam & (1LL << 30)) == 0 && value->activeKeys.empty()) {
      (void)value->host->setMultiContactPolicy(
          canvas::ink_playground::windows_input::nextMultiContactPolicy(
              value->host->multiContactPolicy()));
      InvalidateRect(window, nullptr, FALSE);
    }
    return 0;
  }
  if (message == WM_CHAR && wParam == L' ') {
    // WM_KEYDOWN owns the toggle. Consume the translated character so one
    // physical key press cannot toggle the debug mirror twice.
    return 0;
  }
  if (message == WM_ERASEBKGND) return 1;
  return DefWindowProcW(window, message, wParam, lParam);
}
}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
  WNDCLASSW klass{}; klass.hInstance = instance; klass.lpfnWndProc = WindowProc; klass.lpszClassName = L"AxiomInkPlayground";
  if (RegisterClassW(&klass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
  (void)canvas::ink_playground::windows_input::keepMouseOnButtonMessages();
  State value; value.host = std::make_unique<InkPlaygroundHost>();
#if defined(CANVAS_RENDER_HAS_SKIA)
  value.canonicalRenderer = std::make_unique<canvas::render::SkiaRenderer>();
#endif
  if (!value.host->bindSurface(1024, 768)) return 1;
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto provider = std::make_unique<canvas::render::RasterSkiaSurfaceProvider>();
  if (provider->resize(1024, 768).code != canvas::render::BackendSubmissionCode::kAccepted) return 1;
  if (!value.host->registerSurfaceProvider("windows-gdi", std::move(provider))) return 1;
  value.canonicalProvider = dynamic_cast<canvas::render::RasterSkiaSurfaceProvider*>(
      value.host->activeSurfaceProvider());
  if (value.canonicalProvider == nullptr) return 1;
#endif
  value.window = CreateWindowExW(0, klass.lpszClassName, L"Axiom Ink Playground", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, nullptr, nullptr, instance, &value);
  if (value.window == nullptr) return 1;
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto previewProvider = std::make_unique<canvas::ink_playground::WindowsSkiaPreviewSurfaceProvider>(value.window);
  if (previewProvider->resize(1024, 768).code != canvas::render::BackendSubmissionCode::kAccepted) return 1;
  auto* previewRaw = previewProvider.get();
  if (!value.host->registerPreviewSurfaceProvider("windows-skia-arc-preview",
                                                  std::move(previewProvider))) return 1;
  value.previewProvider = previewRaw;
#endif
  value.previewBridge = std::make_unique<arc::Bridge>(arc::CreateNullBackend(), arc::CreateNullBackend());
  value.runtimeSinks = std::make_unique<WindowsArcRuntimeSinks>(value);
  value.host->setArcPreviewSink(value.runtimeSinks.get());
  value.host->setCanonicalVisibilitySink(value.runtimeSinks.get());
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (!attachPreviewTarget(value, 1024, 768)) return 1;
#endif
  ShowWindow(value.window, show);
  UpdateWindow(value.window);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  return static_cast<int>(message.wParam);
}
#else
int main() { return 2; }
#endif
