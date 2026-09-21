#include "ink_playground_host.hpp"
#include "arc/arc.hpp"
#include "windows_pointer_utils.hpp"
#include "windows_smoke_evidence.hpp"
#include "canvas/render/canonical_handoff.hpp"
#include "canvas/input/active_pointer_registry.hpp"
#if defined(CANVAS_RENDER_HAS_SKIA)
#include "canvas/render/skia_ink_backend.hpp"
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {
using canvas::ink_playground::InkPlaygroundHost;

class ArcSink final : public arc::PointerSampleSink {
 public:
  ArcSink(InkPlaygroundHost& host,
          const std::unordered_map<std::uint64_t, canvas::input::PointerKey>& activeKeys)
      : host_(host), activeKeys_(activeKeys) {}
  arc::Status Push(const arc_pointer_sample_batch_v0& batch) override {
    canvas::input::PointerSampleBatch samples;
    samples.samples.reserve(batch.sample_count);
    const auto* raw = reinterpret_cast<const std::byte*>(batch.samples);
    for (std::uint32_t index = 0; index < batch.sample_count; ++index) {
      const auto* sample = reinterpret_cast<const arc_pointer_sample_v0*>(raw +
          static_cast<std::size_t>(index) * batch.sample_stride);
      const auto key = activeKeys_.find(sample->pointer_id);
      if (key == activeKeys_.end()) return arc::Status::kInvalidState;
      const auto phase = sample->phase == ARC_POINTER_PHASE_DOWN
          ? canvas::input::PointerPhase::kDown
          : sample->phase == ARC_POINTER_PHASE_UP ? canvas::input::PointerPhase::kUp
          : sample->phase == ARC_POINTER_PHASE_CANCEL ? canvas::input::PointerPhase::kCancel
          : canvas::input::PointerPhase::kMove;
      samples.samples.push_back({sample->sample_sequence, sample->timestamp_us * 1000U,
                                 sample->x, sample->y, sample->pressure,
                                 sample->provenance == ARC_SAMPLE_PLATFORM_PREDICTION_HINT,
                                 key->second, {}, {}, phase});
    }
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    return host_.accept(samples, now) ? arc::Status::kOk : arc::Status::kInvalidState;
  }
  void SourceLost(std::uint64_t, arc::Status) override { (void)host_.loseSurface(); }
 private:
  InkPlaygroundHost& host_;
  const std::unordered_map<std::uint64_t, canvas::input::PointerKey>& activeKeys_;
};

struct State { HWND window = nullptr; std::unique_ptr<InkPlaygroundHost> host;
  std::unique_ptr<arc::InputSource> input; std::unique_ptr<arc::Bridge> previewBridge;
  std::unique_ptr<ArcSink> sink; std::uint64_t stroke = 0; std::uint64_t previewGeneration = 1;
  struct ActivePreview { std::uint64_t stroke = 0; std::uint64_t revision = 0;
    std::vector<arc_preview_primitive_v0> points; bool active = false; };
  std::unordered_map<std::uint64_t, ActivePreview> previews;
  struct PendingHandoff { std::uint64_t stroke = 0; std::uint64_t revision = 0; };
  std::vector<PendingHandoff> pendingHandoffs;
  std::uint64_t deviceId = 0;
  canvas::input::ActivePointerRegistry pointerRegistry;
  std::unordered_map<std::uint64_t, canvas::input::PointerKey> activeKeys;
  std::unordered_map<std::uint64_t, std::uint64_t> pointerStrokes;
  std::vector<canvas::ink_playground::windows_input::PointerEvidenceSample> trace;
  std::size_t maxConcurrentPointers = 0;
  bool runtimePreviewVisible = true;
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::SkiaInkBackend> canonicalRenderer;
#endif
};
State* state(HWND window) { return reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA)); }
void persistEvidence(const State& value);

void beginArcPreview(State& value, std::uint64_t pointerId) {
  if (value.previewBridge == nullptr) return;
  auto& preview = value.previews[pointerId];
  preview = State::ActivePreview{value.pointerStrokes.at(pointerId), 0, {}, true};
  arc_preview_begin_v0 begin{}; begin.struct_size = sizeof(begin); begin.abi_version = ARC_ABI_VERSION;
  begin.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; begin.stroke_id = preview.stroke; begin.view_id = 1;
  begin.viewport_revision = 1; begin.target_generation = value.previewGeneration;
  begin.brush.struct_size = sizeof(begin.brush); begin.brush.abi_version = ARC_ABI_VERSION;
  (void)value.previewBridge->Begin(begin);
}

void pushArcPreview(State& value, std::uint64_t pointerId, float x, float y) {
  if (value.previewBridge == nullptr) return;
  auto& preview = value.previews.at(pointerId);
  preview.points.push_back(
      arc_preview_primitive_v0{ARC_PREVIEW_PRIMITIVE_VECTOR_POINT, 0, x, y, 2.0F, 0.0F, 1.0F});
  arc_preview_update_v0 update{}; update.struct_size = sizeof(update); update.abi_version = ARC_ABI_VERSION;
  update.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; update.stroke_id = preview.stroke;
  update.preview_revision = ++preview.revision;
  update.coordinate_space = ARC_COORDINATE_SPACE_DEVICE_PIXEL; update.target_generation = value.previewGeneration;
  update.confirmed_append = preview.points.data();
  update.confirmed_append_count = static_cast<std::uint32_t>(preview.points.size());
  update.confirmed_append_stride = sizeof(arc_preview_primitive_v0);
  (void)value.previewBridge->Push(update);
}

void suppressArcPreview(State& value, std::uint64_t pointerId) {
  const auto preview = value.previews.find(pointerId);
  if (value.previewBridge == nullptr || preview == value.previews.end() ||
      !preview->second.active) {
    return;
  }
  arc_preview_cancel_v0 cancel{sizeof(cancel), ARC_ABI_VERSION, preview->second.stroke,
                               value.previewGeneration, 1U, 0U};
  (void)value.previewBridge->Cancel(cancel);
  preview->second.active = false;
}

void suppressAllArcPreviews(State& value) {
  std::vector<std::uint64_t> pointerIds;
  pointerIds.reserve(value.previews.size());
  for (const auto& [pointerId, preview] : value.previews) {
    if (preview.active) pointerIds.push_back(pointerId);
  }
  for (const auto pointerId : pointerIds) suppressArcPreview(value, pointerId);
}

void commitArcHandoff(State& value, std::uint64_t pointerId) {
  if (value.previewBridge == nullptr) return;
  auto& preview = value.previews.at(pointerId);
  const auto revision = preview.revision;
  if (revision == 0) return;
  arc_preview_seal_v0 seal{sizeof(seal), ARC_ABI_VERSION, preview.stroke, revision,
                           value.previewGeneration};
  if (value.previewBridge->SealInput(seal) != arc::Status::kOk) return;
  arc_canonical_commit_v0 commit{sizeof(commit), ARC_ABI_VERSION, preview.stroke, revision,
                                 preview.stroke, value.previewGeneration, {1, preview.stroke}};
  if (value.previewBridge->CanonicalCommitted(commit) != arc::Status::kOk) return;
  preview.active = false;
  value.pendingHandoffs.push_back({preview.stroke, revision});
  value.host->recordPresentation("canonical-covered", value.pendingHandoffs.size(), 0.0);
}

bool acknowledgeCanonicalVisible(State& value) {
  if (value.previewBridge == nullptr || value.pendingHandoffs.empty()) return false;
  bool released = false;
  std::vector<State::PendingHandoff> stillPending;
  stillPending.reserve(value.pendingHandoffs.size());
  for (const auto& pending : value.pendingHandoffs) {
    arc_canonical_visible_v0 visible{}; visible.struct_size = sizeof(visible);
    visible.abi_version = ARC_ABI_VERSION; visible.stroke_id = pending.stroke;
    visible.document_revision = pending.stroke; visible.target_generation = value.previewGeneration;
    visible.handoff_token = {1, pending.stroke};
    visible.receipt.struct_size = sizeof(visible.receipt); visible.receipt.abi_version = ARC_ABI_VERSION;
    visible.receipt.evidence = ARC_EVIDENCE_DETERMINISTIC_ORACLE; visible.receipt.status = ARC_STATUS_OK;
    visible.receipt.target_generation = value.previewGeneration; visible.receipt.presentation_id = pending.stroke;
    const auto status = value.previewBridge->CanonicalVisible(visible);
    if (status == arc::Status::kOk) {
      released = true;
    } else {
      stillPending.push_back(pending);
    }
  }
  value.pendingHandoffs = std::move(stillPending);
  value.host->recordPresentation(value.pendingHandoffs.empty()
                                     ? "canonical-visible"
                                     : "canonical-visible-retry",
                                 value.pendingHandoffs.size(), 0.0);
  return released;
}

bool renderCanonical(State& value) {
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (value.canonicalRenderer == nullptr) return false;
  std::vector<std::vector<canvas::render::CanonicalStrokePoint>> strokes;
  for (const auto& stroke : value.host->canonicalStrokes()) {
    auto& converted = strokes.emplace_back();
    converted.reserve(stroke.size());
    for (const auto& point : stroke) converted.push_back({point.x, point.y, point.pressure});
  }
  const auto& viewport = value.host->viewportGesture();
  const auto result = value.canonicalRenderer->submit(
      strokes, canvas::render::CanonicalViewportTransform{
          viewport.scale, viewport.translationX, viewport.translationY});
  if (result.code != canvas::render::BackendSubmissionCode::kAccepted) return false;
  if (value.pendingHandoffs.empty()) return true;
  return true;
#else
  return false;
#endif
}

bool finalizeCanonicalPresentation(State& value) {
  if (value.pendingHandoffs.empty()) return true;
  const auto surface = value.host->surface();
  for (const auto& pending : value.pendingHandoffs) {
    if (!value.host->presentCanonicalFrame(pending.stroke, 0.0)) return false;
    canvas::render::HandoffEligibility eligibility{};
    eligibility.tokenHigh = 1U;
    eligibility.tokenLow = pending.stroke;
    eligibility.pending = true;
    eligibility.requiredDocumentRevision = pending.stroke;
    eligibility.presentedDocumentRevision = pending.stroke;
    eligibility.requiredSurfaceGeneration = surface.generation;
    eligibility.presentedSurfaceGeneration = surface.generation;
    eligibility.requiredMetricsGeneration = surface.generation;
    eligibility.presentedMetricsGeneration = surface.generation;
    eligibility.presented = true;
    eligibility.platformQualified = true;
    eligibility.coverageEligible = true;
    if (canvas::render::CanonicalHandoffEvaluator::evaluate(eligibility) !=
        canvas::render::HandoffDisposition::kEligible) return false;
  }
  return acknowledgeCanonicalVisible(value);
}

bool submitMouseSample(HWND window, State& value, UINT message, WPARAM wParam, LPARAM lParam) {
  constexpr UINT kMousePointerId = 1U;
  const auto timestampMs = static_cast<std::uint64_t>(GetMessageTime());
  bool begin = false;
  bool end = false;
  std::uint32_t phase = ARC_POINTER_PHASE_MOVE;
  if (message == WM_LBUTTONDOWN) {
    const auto key = value.pointerRegistry.begin(1, kMousePointerId);
    if (!key.valid()) return false;
    value.activeKeys[kMousePointerId] = key;
    value.maxConcurrentPointers = (std::max)(value.maxConcurrentPointers, value.activeKeys.size());
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
    value.pointerStrokes[kMousePointerId] = value.stroke;
    if (!value.host->beginStroke(value.activeKeys.at(kMousePointerId), value.stroke)) return false;
    beginArcPreview(value, kMousePointerId);
    SetCapture(window);
  }

  const auto x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
  const auto y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
  arc_pointer_sample_v0 sample{};
  sample.pointer_id = kMousePointerId;
  sample.sample_sequence = (timestampMs << 16U) | phase;
  sample.timestamp_us = timestampMs * 1000U;
  sample.x = x;
  sample.y = y;
  sample.pressure = 0.5F;
  sample.phase = phase;
  sample.provenance = ARC_SAMPLE_CONFIRMED_CURRENT;
  arc_pointer_sample_batch_v0 batch{};
  batch.struct_size = sizeof(batch);
  batch.abi_version = ARC_ABI_VERSION;
  batch.schema_version = ARC_PROTOCOL_SCHEMA_VERSION;
  batch.coordinate_space = ARC_COORDINATE_SPACE_DEVICE_PIXEL;
  batch.view_id = 1;
  batch.viewport_revision = 1;
  batch.device_id = kMousePointerId;
  batch.input_capabilities = 0;
  batch.tool = ARC_INPUT_TOOL_MOUSE;
  batch.samples = &sample;
  batch.sample_count = 1;
  batch.sample_stride = sizeof(sample);
  if (value.input->SubmitBatch(batch) != arc::Status::kOk) return false;
  pushArcPreview(value, kMousePointerId, x, y);
  value.deviceId = kMousePointerId;
  const auto key = value.activeKeys.at(kMousePointerId);
  value.trace.push_back({key.source, kMousePointerId, key.generation, timestampMs, "mouse",
                          phase == ARC_POINTER_PHASE_DOWN ? "down" :
                          (phase == ARC_POINTER_PHASE_UP ? "up" : "move"),
                          x, y, sample.pressure, 1U, 0.0F, 0.0F});
  persistEvidence(value);
  if (end) {
    const auto stroke = value.pointerStrokes.at(kMousePointerId);
    if (!value.host->commitStroke(value.activeKeys.at(kMousePointerId), stroke, stroke)) return false;
    commitArcHandoff(value, kMousePointerId);
    (void)value.pointerRegistry.end(value.activeKeys.at(kMousePointerId));
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
    const auto preview = value.previews.find(pointerId);
    if (value.previewBridge != nullptr && preview != value.previews.end()) {
      arc_preview_cancel_v0 cancel{sizeof(cancel), ARC_ABI_VERSION, preview->second.stroke,
                                   value.previewGeneration, 1U, 0U};
      (void)value.previewBridge->Cancel(cancel);
    }
    (void)value.host->cancelStroke(key);
  }
  value.trace.push_back({key.source, static_cast<std::uint32_t>(pointerId), key.generation,
                         timestampMs, "unknown", "cancel", 0.0F, 0.0F, 0.0F, 0U,
                         0.0F, 0.0F});
  (void)value.pointerRegistry.end(key);
  value.activeKeys.erase(pointerId);
  value.pointerStrokes.erase(pointerId);
  value.previews.erase(pointerId);
  persistEvidence(value);
}

void cancelAllPointers(State& value, std::uint64_t timestampMs) {
  suppressAllArcPreviews(value);
  for (const auto& [pointerId, key] : value.activeKeys) {
    value.trace.push_back({key.source, static_cast<std::uint32_t>(pointerId), key.generation,
                           timestampMs, "unknown", "cancel", 0.0F, 0.0F, 0.0F, 0U,
                           0.0F, 0.0F});
    (void)value.pointerRegistry.end(key);
  }
  value.host->cancelAllPointers();
  value.activeKeys.clear();
  value.pointerStrokes.clear();
  value.previews.clear();
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
          << value.host->viewportGesture().translationY << "\n}\n";
}

void paint(HWND window, State& value) {
  PAINTSTRUCT ps{}; HDC dc = BeginPaint(window, &ps); RECT rect{}; GetClientRect(window, &rect);
  HDC bufferDc = CreateCompatibleDC(dc);
  HBITMAP bitmap = CreateCompatibleBitmap(dc, rect.right - rect.left, rect.bottom - rect.top);
  HGDIOBJ oldBitmap = SelectObject(bufferDc, bitmap);
  FillRect(bufferDc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  bool canonicalPresented = false;
#if defined(CANVAS_RENDER_HAS_SKIA)
  (void)value.canonicalRenderer->resize(static_cast<std::uint32_t>(rect.right),
                                        static_cast<std::uint32_t>(rect.bottom));
  canonicalPresented = renderCanonical(value);
  const auto pixels = value.canonicalRenderer->rgba();
  if (!pixels.empty()) {
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(value.canonicalRenderer->width());
    info.bmiHeader.biHeight = -static_cast<LONG>(value.canonicalRenderer->height());
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<std::uint8_t> bgra(pixels.begin(), pixels.end());
    for (std::size_t i = 0; i + 3 < bgra.size(); i += 4) std::swap(bgra[i], bgra[i + 2]);
    SetDIBitsToDevice(bufferDc, 0, 0, value.canonicalRenderer->width(),
                      value.canonicalRenderer->height(), 0, 0, 0,
                      value.canonicalRenderer->height(), bgra.data(), &info, DIB_RGB_COLORS);
  }
#endif
  HPEN pen = CreatePen(PS_SOLID, 3, RGB(26, 91, 255)); HGDIOBJ oldPen = SelectObject(bufferDc, pen);
  SetBkMode(bufferDc, TRANSPARENT);
  SetTextColor(bufferDc, RGB(30, 30, 30));
  const auto& hud = value.host->hud();
  std::wstringstream status;
  const bool previewInputActive = std::any_of(value.previews.begin(), value.previews.end(),
      [](const auto& item) { return item.second.active; });
  const wchar_t* qualificationState = previewInputActive
                                          ? L"PREVIEW_ONLY"
                                          : (!value.pendingHandoffs.empty() ? L"OVERLAP"
                                                                            : L"CANONICAL_ONLY");
  status << L"Axiom Ink Playground | HWND ready | WM_POINTER enabled | samples: "
         << value.trace.size() << L" | batch: " << hud.batch
         << L" | pointers: " << value.activeKeys.size()
         << L" | mode: " << canvas::ink_playground::windows_input::multiContactPolicyName(
                                value.host->multiContactPolicy())
         << L" | viewport: " << value.host->viewportGesture().scale << L"x @ "
         << value.host->viewportGesture().translationX << L","
         << value.host->viewportGesture().translationY
         << L" | pending: " << value.pendingHandoffs.size()
         << L" | state: " << qualificationState
         << L" | Arc preview: native layered"
         << L" | runtime preview: " << (value.runtimePreviewVisible ? L"ON" : L"OFF")
         << L" | SPACE = preview | P = pointer mode";
  const auto text = status.str();
  TextOutW(bufferDc, 16, 16, text.c_str(), static_cast<int>(text.size()));
  // Optional presentation-only mirror. It is never used for canonical
  // handoff eligibility and is off by default so Arc evidence cannot be
  // satisfied by this debug layer.
  if (value.runtimePreviewVisible && previewInputActive)
      for (const auto& [pointerId, preview] : value.previews) {
    static_cast<void>(pointerId);
    if (!preview.active) continue;
    for (std::size_t i = 1; i < preview.points.size(); ++i) {
      MoveToEx(bufferDc, static_cast<int>(preview.points[i - 1].x), static_cast<int>(preview.points[i - 1].y), nullptr);
      LineTo(bufferDc, static_cast<int>(preview.points[i].x), static_cast<int>(preview.points[i].y));
    }
  }
  BitBlt(dc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, bufferDc, 0, 0, SRCCOPY);
  // The Arc handoff is released only after the Skia-produced canonical pixels
  // have been copied to the real HWND. Space never participates in this path.
  if (canonicalPresented) {
    (void)finalizeCanonicalPresentation(value);
  }
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
    if (value != nullptr && value->input != nullptr) {
      (void)submitMouseSample(window, *value, message, wParam, lParam);
    }
    return 0;
  }
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP) {
    if (value == nullptr || value->input == nullptr) return 0;
    const UINT pointerId = GET_POINTERID_WPARAM(wParam);
    POINTER_INFO info{};
    if (!GetPointerInfo(pointerId, &info)) return 0;
    if (info.pointerType == PT_MOUSE) return 0;
    const auto phase = message == WM_POINTERDOWN ? ARC_POINTER_PHASE_DOWN :
        (message == WM_POINTERUP ? ARC_POINTER_PHASE_UP : ARC_POINTER_PHASE_MOVE);
    bool begin = false;
    bool end = false;
    if (phase == ARC_POINTER_PHASE_DOWN) {
      const auto source = canvas::ink_playground::windows_input::functionalDeviceId(
          info.sourceDevice, static_cast<std::uint64_t>(pointerId));
      const auto key = value->pointerRegistry.begin(source, pointerId);
      if (!key.valid()) return 0;
      value->activeKeys[pointerId] = key;
      value->maxConcurrentPointers = (std::max)(value->maxConcurrentPointers,
                                                value->activeKeys.size());
      begin = true;
    } else if (phase == ARC_POINTER_PHASE_UP) {
      end = value->activeKeys.contains(pointerId);
    } else if (!value->activeKeys.contains(pointerId)) {
      return 0;
    }
    if (begin) {
      ++value->stroke;
      value->pointerStrokes[pointerId] = value->stroke;
      if (!value->host->beginStroke(value->activeKeys.at(pointerId), value->stroke)) return 0;
      beginArcPreview(*value, pointerId);
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
    std::vector<arc_pointer_sample_v0> samples;
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
      arc_pointer_sample_v0 sample{};
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
      const auto key = value->activeKeys.at(pointerId);
      value->trace.push_back({key.source, pointerId, key.generation, history.dwTime,
                              info.pointerType == PT_PEN ? "pen" :
                              (info.pointerType == PT_TOUCH ? "touch" : "mouse"),
                              historyPhase == ARC_POINTER_PHASE_DOWN ? "down" :
                              (historyPhase == ARC_POINTER_PHASE_UP ? "up" : "move"),
                               sample.x, sample.y, sample.pressure, pointerHistory.size(),
                               contactWidth, contactHeight});
    }
    arc_pointer_sample_batch_v0 batch{}; batch.struct_size = sizeof(batch); batch.abi_version = ARC_ABI_VERSION;
    batch.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; batch.coordinate_space = ARC_COORDINATE_SPACE_DEVICE_PIXEL;
    batch.view_id = 1; batch.viewport_revision = 1;
    value->deviceId = canvas::ink_playground::windows_input::functionalDeviceId(
        info.sourceDevice, static_cast<std::uint64_t>(pointerId));
    batch.device_id = value->deviceId;
    batch.input_capabilities = ARC_INPUT_CAPABILITY_HISTORY;
    if (info.pointerType == PT_PEN) batch.input_capabilities |= ARC_INPUT_CAPABILITY_PRESSURE;
    batch.tool = canvas::ink_playground::windows_input::arcToolForPointerType(info.pointerType);
    batch.samples = samples.data(); batch.sample_count = static_cast<std::uint32_t>(samples.size());
    batch.sample_stride = sizeof(arc_pointer_sample_v0);
    if (value->input->SubmitBatch(batch) != arc::Status::kOk) return 0;
    const auto key = value->activeKeys.at(pointerId);
    const auto disposition = value->host->pointerDisposition(key);
    const auto action = canvas::ink_playground::windows_input::platformPointerAction(
        disposition, end);
    if (action == canvas::ink_playground::windows_input::PlatformPointerAction::kSuppressPreview ||
        action == canvas::ink_playground::windows_input::PlatformPointerAction::kReleaseWithoutCommit) {
      suppressAllArcPreviews(*value);
    } else if (!samples.empty()) {
      pushArcPreview(*value, pointerId, samples.back().x, samples.back().y);
    }
    persistEvidence(*value);
    if (end) {
      if (action == canvas::ink_playground::windows_input::PlatformPointerAction::kCommitAndRelease) {
        const auto stroke = value->pointerStrokes.at(pointerId);
        if (!value->host->commitStroke(key, stroke, stroke)) return 0;
        commitArcHandoff(*value, pointerId);
      }
      (void)value->pointerRegistry.end(key);
      value->activeKeys.erase(pointerId);
      value->pointerStrokes.erase(pointerId);
      value->previews.erase(pointerId);
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
      if (value != nullptr && value->input != nullptr) value->input->Stop();
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
  value.canonicalRenderer = std::make_unique<canvas::render::SkiaInkBackend>();
#endif
  if (!value.host->bindSurface(1024, 768)) return 1;
  value.input = arc::CreateWindowsInputSource();
  value.previewBridge = std::make_unique<arc::Bridge>(arc::CreateWindowsBackend(), arc::CreateNullBackend());
  value.sink = std::make_unique<ArcSink>(*value.host, value.activeKeys);
  if (value.input == nullptr || value.input->Start(*value.sink) != arc::Status::kOk) return 1;
  value.window = CreateWindowExW(0, klass.lpszClassName, L"Axiom Ink Playground", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, nullptr, nullptr, instance, &value);
  if (value.window == nullptr) return 1; ShowWindow(value.window, show); UpdateWindow(value.window);
  if (value.previewBridge != nullptr) {
    arc_preview_target_v0 target{}; target.struct_size = sizeof(target); target.abi_version = ARC_ABI_VERSION;
    target.platform_kind = ARC_PLATFORM_WINDOWS; target.target_id = 1; target.target_generation = value.previewGeneration;
    target.width_pixels = 1024; target.height_pixels = 768; target.device_pixel_ratio = 1.0F;
    target.opaque_platform_handle = reinterpret_cast<std::uint64_t>(value.window);
    if (value.previewBridge->Attach(target) != arc::Status::kOk) return 1;
  }
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  return static_cast<int>(message.wParam);
}
#else
int main() { return 2; }
#endif
