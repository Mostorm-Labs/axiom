#include "ink_playground_host.hpp"
#include "arc/arc.hpp"
#include "windows_pointer_utils.hpp"
#include "windows_smoke_evidence.hpp"

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
#include <vector>

namespace {
using canvas::ink_playground::InkPlaygroundHost;

class ArcSink final : public arc::PointerSampleSink {
 public:
  explicit ArcSink(InkPlaygroundHost& host) : host_(host) {}
  arc::Status Push(const arc_pointer_sample_batch_v0& batch) override {
    canvas::input::PointerSampleBatch samples;
    samples.samples.reserve(batch.sample_count);
    const auto* raw = reinterpret_cast<const std::byte*>(batch.samples);
    for (std::uint32_t index = 0; index < batch.sample_count; ++index) {
      const auto* sample = reinterpret_cast<const arc_pointer_sample_v0*>(raw +
          static_cast<std::size_t>(index) * batch.sample_stride);
      samples.samples.push_back({sample->sample_sequence, sample->timestamp_us * 1000U,
                                 sample->x, sample->y, sample->pressure,
                                 sample->provenance == ARC_SAMPLE_PLATFORM_PREDICTION_HINT});
    }
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    return host_.accept(samples, now) ? arc::Status::kOk : arc::Status::kInvalidState;
  }
  void SourceLost(std::uint64_t, arc::Status) override { (void)host_.loseSurface(); }
 private:
  InkPlaygroundHost& host_;
};

struct State { HWND window = nullptr; std::unique_ptr<InkPlaygroundHost> host;
  std::unique_ptr<arc::InputSource> input; std::unique_ptr<ArcSink> sink; std::uint64_t stroke = 0;
  std::uint64_t deviceId = 0; std::vector<canvas::ink_playground::windows_input::PointerEvidenceSample> trace; };
State* state(HWND window) { return reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA)); }

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
          << "  \"frame_time_ms\": " << hud.frameMs << "\n}\n";
}

void paint(HWND window, State& value) {
  PAINTSTRUCT ps{}; HDC dc = BeginPaint(window, &ps); RECT rect{}; GetClientRect(window, &rect);
  HDC bufferDc = CreateCompatibleDC(dc);
  HBITMAP bitmap = CreateCompatibleBitmap(dc, rect.right - rect.left, rect.bottom - rect.top);
  HGDIOBJ oldBitmap = SelectObject(bufferDc, bitmap);
  FillRect(bufferDc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  HPEN pen = CreatePen(PS_SOLID, 3, RGB(26, 91, 255)); HGDIOBJ oldPen = SelectObject(bufferDc, pen);
  SetBkMode(bufferDc, TRANSPARENT);
  SetTextColor(bufferDc, RGB(30, 30, 30));
  const auto& hud = value.host->hud();
  std::wstringstream status;
  status << L"Axiom Ink Playground | HWND ready | WM_POINTER enabled | samples: "
         << value.trace.size() << L" | batch: " << hud.batch;
  const auto text = status.str();
  TextOutW(bufferDc, 16, 16, text.c_str(), static_cast<int>(text.size()));
  for (const auto& points : value.host->previewStrokes()) {
    for (std::size_t i = 1; i < points.size(); ++i) {
      MoveToEx(bufferDc, static_cast<int>(points[i - 1].x), static_cast<int>(points[i - 1].y), nullptr);
      LineTo(bufferDc, static_cast<int>(points[i].x), static_cast<int>(points[i].y));
    }
  }
  BitBlt(dc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, bufferDc, 0, 0, SRCCOPY);
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
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP) {
    if (value == nullptr || value->input == nullptr) return 0;
    const UINT pointerId = GET_POINTERID_WPARAM(wParam);
    POINTER_INFO info{};
    if (!GetPointerInfo(pointerId, &info)) return 0;
    const auto phase = message == WM_POINTERDOWN ? ARC_POINTER_PHASE_DOWN :
        (message == WM_POINTERUP ? ARC_POINTER_PHASE_UP : ARC_POINTER_PHASE_MOVE);
    if (phase == ARC_POINTER_PHASE_DOWN) { ++value->stroke; if (!value->host->beginStroke(value->stroke)) return 0; }
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
    pointerHistory = canvas::ink_playground::windows_input::normalizePointerHistory(
        pointerHistory, phase == ARC_POINTER_PHASE_DOWN);
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
      value->trace.push_back({pointerId, history.dwTime,
                              info.pointerType == PT_PEN ? "pen" :
                              (info.pointerType == PT_TOUCH ? "touch" : "mouse"),
                              historyPhase == ARC_POINTER_PHASE_DOWN ? "down" :
                              (historyPhase == ARC_POINTER_PHASE_UP ? "up" : "move"),
                              sample.x, sample.y, sample.pressure, pointerHistory.size()});
    }
    arc_pointer_sample_batch_v0 batch{}; batch.struct_size = sizeof(batch); batch.abi_version = ARC_ABI_VERSION;
    batch.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; batch.coordinate_space = ARC_COORDINATE_SPACE_DEVICE_PIXEL;
    batch.view_id = 1; batch.viewport_revision = 1;
    value->deviceId = canvas::ink_playground::windows_input::functionalDeviceId(
        info.sourceDevice, static_cast<std::uint64_t>(pointerId));
    batch.device_id = value->deviceId;
    batch.input_capabilities = ARC_INPUT_CAPABILITY_PRESSURE | ARC_INPUT_CAPABILITY_HISTORY;
    batch.tool = info.pointerType == PT_PEN ? ARC_INPUT_TOOL_PEN : ARC_INPUT_TOOL_MOUSE;
    batch.samples = samples.data(); batch.sample_count = static_cast<std::uint32_t>(samples.size());
    batch.sample_stride = sizeof(arc_pointer_sample_v0);
    if (value->input->SubmitBatch(batch) != arc::Status::kOk) return 0;
    persistEvidence(*value);
    if (phase == ARC_POINTER_PHASE_UP && !value->host->commitStroke(value->stroke, value->stroke)) return 0;
    InvalidateRect(window, nullptr, FALSE); return 0;
  }
  if (message == WM_PAINT) { if (value != nullptr) paint(window, *value); return 0; }
  if (message == WM_ERASEBKGND) return 1;
  if (message == WM_DESTROY) { if (value != nullptr && value->input != nullptr) value->input->Stop(); PostQuitMessage(0); return 0; }
  return DefWindowProcW(window, message, wParam, lParam);
}
}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
  WNDCLASSW klass{}; klass.hInstance = instance; klass.lpfnWndProc = WindowProc; klass.lpszClassName = L"AxiomInkPlayground";
  if (RegisterClassW(&klass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
  (void)canvas::ink_playground::windows_input::enableFunctionalPointerIngress();
  State value; value.host = std::make_unique<InkPlaygroundHost>();
  if (!value.host->bindSurface(1024, 768)) return 1;
  value.input = arc::CreateWindowsInputSource(); value.sink = std::make_unique<ArcSink>(*value.host);
  if (value.input == nullptr || value.input->Start(*value.sink) != arc::Status::kOk) return 1;
  value.window = CreateWindowExW(0, klass.lpszClassName, L"Axiom Ink Playground", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, nullptr, nullptr, instance, &value);
  if (value.window == nullptr) return 1; ShowWindow(value.window, show); UpdateWindow(value.window);
  MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  return static_cast<int>(message.wParam);
}
#else
int main() { return 2; }
#endif
