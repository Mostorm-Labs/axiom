#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "arc/arc.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace arc {
namespace {
constexpr wchar_t kClassName[] = L"AxiomArcPreviewSurface";
struct Stroke {
  std::vector<arc_preview_primitive_v0> confirmed;
  std::vector<arc_preview_primitive_v0> predicted;
  arc_handoff_token_v0 token{};
  bool sealed = false;
  bool committed = false;
};
LRESULT CALLBACK PreviewProc(HWND w, UINT m, WPARAM wparam, LPARAM lparam) {
  if (m == WM_NCHITTEST) return HTTRANSPARENT;
  if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
  return DefWindowProcW(w, m, wparam, lparam);
}
bool RegisterPreviewClass() {
  static bool done = false;
  if (done) return true;
  WNDCLASSW c{}; c.hInstance = GetModuleHandleW(nullptr); c.lpfnWndProc = PreviewProc;
  c.lpszClassName = kClassName;
  if (RegisterClassW(&c) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  done = true; return true;
}

class WindowsPreviewBackend final : public PreviewBackend {
 public:
  ~WindowsPreviewBackend() override { destroy(); }
  arc_backend_capabilities_v0 Capabilities() const override {
    return {sizeof(arc_backend_capabilities_v0), ARC_ABI_VERSION, ARC_PLATFORM_WINDOWS, 0,
            0, ARC_PRESENTATION_CAPABILITY_INDEPENDENT_TARGET |
                ARC_PRESENTATION_CAPABILITY_REPLACE_TRUNCATE |
                ARC_PRESENTATION_CAPABILITY_PRESENT_RECEIPT |
                ARC_PRESENTATION_CAPABILITY_SHARED_GPU_DEVICE,
            64, 1u << 20u, 64u * 1024u * 1024u};
  }
  Status Attach(const arc_preview_target_v0& t) override {
    if (t.platform_kind != ARC_PLATFORM_WINDOWS || t.target_generation == 0 ||
        t.target_id == 0 || t.opaque_platform_handle == 0 || t.width_pixels == 0 ||
        t.height_pixels == 0) return Status::kInvalidArgument;
    if (attached_ && t.target_generation < generation_) return Status::kStaleRevision;
    owner_ = reinterpret_cast<HWND>(t.opaque_platform_handle);
    if (!IsWindow(owner_)) {
      target_ = t; generation_ = t.target_generation; attached_ = true; test_only_ = true;
      return Status::kOk;
    }
    test_only_ = false;
    if (!RegisterPreviewClass()) return Status::kBackendUnavailable;
    target_ = t; generation_ = t.target_generation;
    if (surface_ == nullptr) {
      surface_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
                                     WS_EX_TOOLWINDOW, kClassName, L"Axiom Arc Preview",
                                 WS_POPUP, 0, 0, static_cast<int>(t.width_pixels),
                                 static_cast<int>(t.height_pixels), owner_, nullptr,
                                 GetModuleHandleW(nullptr), nullptr);
    }
    attached_ = surface_ != nullptr;
    if (!attached_) return Status::kBackendUnavailable;
    (void)SetWindowPos(surface_, HWND_TOP, 0, 0, static_cast<int>(target_.width_pixels),
                       static_cast<int>(target_.height_pixels), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return Status::kOk;
  }
  Status Detach(uint64_t generation) override {
    if (!attached_ || generation != generation_) return Status::kStaleRevision;
    destroy(); return Status::kOk;
  }
  Status Begin(const arc_preview_begin_v0& b) override {
    if (!attached_ || b.target_generation != generation_) return Status::kStaleRevision;
    strokes_[b.stroke_id] = Stroke{}; return Status::kOk;
  }
  Status Push(const arc_preview_update_v0& u) override {
    if (!attached_ || u.target_generation != generation_) return Status::kStaleRevision;
    auto it = strokes_.find(u.stroke_id); if (it == strokes_.end()) return Status::kNotFound;
    if (u.truncate_confirmed_to > it->second.confirmed.size()) return Status::kInvalidArgument;
    it->second.confirmed.resize(u.truncate_confirmed_to);
    append(it->second.confirmed, u.confirmed_append, u.confirmed_append_count,
           u.confirmed_append_stride);
    it->second.predicted.clear();
    append(it->second.predicted, u.predicted_tail, u.predicted_tail_count,
           u.predicted_tail_stride);
    return render() ? Status::kOk : Status::kPresentationFailed;
  }
  Status SealInput(const arc_preview_seal_v0& s) override {
    if (!attached_ || s.target_generation != generation_) return Status::kStaleRevision;
    auto it = strokes_.find(s.stroke_id); if (it == strokes_.end()) return Status::kNotFound;
    it->second.sealed = true; return render() ? Status::kOk : Status::kPresentationFailed;
  }
  Status CanonicalCommitted(const arc_canonical_commit_v0& c) override {
    if (!attached_ || c.target_generation != generation_) return Status::kStaleRevision;
    auto it = strokes_.find(c.stroke_id); if (it == strokes_.end() || !it->second.sealed)
      return Status::kInvalidState;
    it->second.token = c.handoff_token; it->second.committed = true; return Status::kOk;
  }
  Status CanonicalVisible(const arc_canonical_visible_v0& v) override {
    if (!attached_ || v.target_generation != generation_) return Status::kStaleRevision;
    auto it = strokes_.find(v.stroke_id);
    if (it == strokes_.end() || !it->second.committed ||
        it->second.token.high != v.handoff_token.high || it->second.token.low != v.handoff_token.low)
      return Status::kInvalidState;
    strokes_.erase(it); return render() ? Status::kOk : Status::kPresentationFailed;
  }
  Status Cancel(const arc_preview_cancel_v0& c) override {
    if (!attached_ || c.target_generation != generation_) return Status::kStaleRevision;
    strokes_.erase(c.stroke_id); return render() ? Status::kOk : Status::kPresentationFailed;
  }
 private:
  static void append(std::vector<arc_preview_primitive_v0>& out,
                     const arc_preview_primitive_v0* p, uint32_t n, uint32_t stride) {
    if (p == nullptr) return;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(p);
    for (uint32_t i = 0; i < n; ++i) { arc_preview_primitive_v0 v{};
      std::memcpy(&v, bytes + static_cast<size_t>(i) * stride, sizeof(v)); out.push_back(v); }
  }
  bool render() {
    if (test_only_) return true;
    if (!attached_ || surface_ == nullptr) return false;
    POINT origin{0, 0}; if (!ClientToScreen(owner_, &origin)) return false;
    SetWindowPos(surface_, HWND_TOP, origin.x, origin.y, static_cast<int>(target_.width_pixels),
                 static_cast<int>(target_.height_pixels), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    HDC screen = GetDC(nullptr), mem = CreateCompatibleDC(screen); BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = target_.width_pixels;
    bi.bmiHeader.biHeight = -static_cast<LONG>(target_.height_pixels); bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB; void* bits = nullptr;
    HBITMAP bm = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bm || !bits) { if (bm) DeleteObject(bm); DeleteDC(mem); ReleaseDC(nullptr, screen); return false; }
    const size_t count = static_cast<size_t>(target_.width_pixels) * target_.height_pixels;
    std::fill_n(static_cast<std::uint32_t*>(bits), count, 0u); HGDIOBJ old = SelectObject(mem, bm);
    for (const auto& [id, s] : strokes_) { (void)id; draw(mem, s.confirmed); draw(mem, s.predicted); }
    auto* rgba = static_cast<std::uint32_t*>(bits);
    for (size_t i = 0; i < count; ++i) {
      if ((rgba[i] & 0x00ffffffu) != 0) rgba[i] = (rgba[i] & 0x00ffffffu) | 0x88000000u;
    }
    SIZE size{static_cast<LONG>(target_.width_pixels), static_cast<LONG>(target_.height_pixels)};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    POINT source{0, 0};
    const BOOL ok = UpdateLayeredWindow(surface_, screen, &origin, &size, mem, &source, 0,
                                        &blend, ULW_ALPHA);
    SelectObject(mem, old); DeleteObject(bm); DeleteDC(mem); ReleaseDC(nullptr, screen); return ok != FALSE;
  }
  static void draw(HDC dc, const std::vector<arc_preview_primitive_v0>& p) {
    if (p.empty()) return; HPEN pen = CreatePen(PS_SOLID, 8, RGB(40, 190, 235)); HGDIOBJ old = SelectObject(dc, pen);
    POINT prev{}; bool have = false; for (const auto& v : p) { POINT cur{(LONG)v.x, (LONG)v.y};
      if (have) { MoveToEx(dc, prev.x, prev.y, nullptr); LineTo(dc, cur.x, cur.y); }
      prev = cur; have = true; } SelectObject(dc, old); DeleteObject(pen);
  }
  void destroy() { if (surface_) DestroyWindow(surface_); surface_ = nullptr; owner_ = nullptr;
    attached_ = false; test_only_ = false; generation_ = 0; strokes_.clear(); }
  HWND owner_ = nullptr, surface_ = nullptr; arc_preview_target_v0 target_{}; uint64_t generation_ = 0;
  bool attached_ = false; bool test_only_ = false; std::unordered_map<uint64_t, Stroke> strokes_;
};
} // namespace
std::unique_ptr<PreviewBackend> CreateWindowsBackend() { return std::make_unique<WindowsPreviewBackend>(); }

namespace {
class WindowsInputSource final : public InputSource {
 public:
  arc_backend_capabilities_v0 Capabilities() const override {
    return {sizeof(arc_backend_capabilities_v0), ARC_ABI_VERSION, ARC_PLATFORM_WINDOWS, 0,
            ARC_INPUT_CAPABILITY_PRESSURE | ARC_INPUT_CAPABILITY_TILT |
                ARC_INPUT_CAPABILITY_CONTACT | ARC_INPUT_CAPABILITY_HISTORY |
                ARC_INPUT_CAPABILITY_HOVER | ARC_INPUT_CAPABILITY_ERASER,
            0, 64, 1u << 20u, 64u * 1024u * 1024u};
  }
  Status Start(PointerSampleSink& sink) override {
    if (sink_ != nullptr) return Status::kInvalidState; sink_ = &sink; return Status::kOk;
  }
  Status Stop() override { sink_ = nullptr; return Status::kOk; }
  Status SubmitBatch(const arc_pointer_sample_batch_v0& batch) override {
    if (sink_ == nullptr) return Status::kInvalidState;
    if (batch.struct_size < sizeof(batch) || batch.abi_version != ARC_ABI_VERSION ||
        batch.schema_version != ARC_PROTOCOL_SCHEMA_VERSION || batch.device_id == 0 ||
        batch.samples == nullptr || batch.sample_count == 0 ||
        batch.sample_stride < sizeof(arc_pointer_sample_v0)) return Status::kInvalidArgument;
    device_ = batch.device_id; return sink_->Push(batch);
  }
  void NotifySourceLost(Status reason) override { if (sink_) sink_->SourceLost(device_, reason); }
 private: PointerSampleSink* sink_ = nullptr; uint64_t device_ = 0;
};
} // namespace
std::unique_ptr<InputSource> CreateWindowsInputSource() { return std::make_unique<WindowsInputSource>(); }
} // namespace arc
