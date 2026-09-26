#if !defined(_WIN32)
// The native Win32 implementation must only be compiled by a Windows
// toolchain. Other hosts still build every Arc platform target as a portable
// conformance surface, so use the protocol-only backend factory there.
#define ARC_PLATFORM_KIND ARC_PLATFORM_WINDOWS
#define ARC_CREATE_BACKEND CreateWindowsBackend
#define ARC_CREATE_INPUT_SOURCE CreateWindowsInputSource
#define ARC_REQUIRES_PLATFORM_HANDLE 1
#define ARC_INPUT_CAPABILITIES \
  (ARC_INPUT_CAPABILITY_PRESSURE | ARC_INPUT_CAPABILITY_TILT | \
   ARC_INPUT_CAPABILITY_CONTACT | ARC_INPUT_CAPABILITY_HISTORY | \
   ARC_INPUT_CAPABILITY_HOVER | ARC_INPUT_CAPABILITY_ERASER)
#define ARC_PRESENTATION_CAPABILITIES \
  (ARC_PRESENTATION_CAPABILITY_INDEPENDENT_TARGET | \
   ARC_PRESENTATION_CAPABILITY_REPLACE_TRUNCATE | \
   ARC_PRESENTATION_CAPABILITY_PRESENT_RECEIPT | \
   ARC_PRESENTATION_CAPABILITY_SHARED_GPU_DEVICE)
#include "../backend_factory.inc"
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "arc/arc.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <charconv>
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
  uint64_t resourceId = 0;
};
struct Resource {
  uint64_t contentHash = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<std::uint8_t> alpha;
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

// Deprecated compatibility adapter.  The Ink Playground production path no
// longer instantiates this primitive/GDI renderer: Windows preview pixels are
// produced by the Runtime Skia adapter and this ARC backend is retained only
// for ABI/conformance tests and older consumers.
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
    if (target_.width_pixels != t.width_pixels || target_.height_pixels != t.height_pixels)
      releaseBitmap();
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
    ShowWindow(surface_, SW_HIDE);
    (void)SetWindowPos(surface_, HWND_TOP, 0, 0, static_cast<int>(target_.width_pixels),
                       static_cast<int>(target_.height_pixels), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return Status::kOk;
  }
  Status UploadResource(const arc_preview_resource_v0& resource) override {
    if (resource.resource_id == 0 || resource.content_hash == 0 ||
        resource.alpha == nullptr || resource.width == 0 || resource.height == 0 ||
        resource.alpha_size != resource.width * resource.height) return Status::kInvalidArgument;
    auto& cached = resources_[resource.resource_id];
    if (cached.contentHash != 0 && cached.contentHash != resource.content_hash)
      return Status::kInvalidState;
    cached.contentHash = resource.content_hash;
    cached.width = resource.width;
    cached.height = resource.height;
    cached.alpha.assign(resource.alpha, resource.alpha + resource.alpha_size);
    return Status::kOk;
  }
  Status Detach(uint64_t generation) override {
    if (!attached_ || generation != generation_) return Status::kStaleRevision;
    destroy(); return Status::kOk;
  }
  Status Begin(const arc_preview_begin_v0& b) override {
    if (!attached_ || b.target_generation != generation_) return Status::kStaleRevision;
    Stroke stroke{};
    if (b.brush.resource_id != nullptr && b.brush.resource_id_size != 0) {
      const auto* begin = b.brush.resource_id;
      const auto* end = begin + b.brush.resource_id_size;
      const auto parsed = std::from_chars(begin, end, stroke.resourceId);
      if (parsed.ec != std::errc{} || parsed.ptr != end) return Status::kInvalidArgument;
    }
    if (stroke.resourceId != 0 && !resources_.contains(stroke.resourceId))
      return Status::kNotFound;
    strokes_[b.stroke_id] = std::move(stroke); return Status::kOk;
  }
  Status Push(const arc_preview_update_v0& u) override {
    if (!attached_ || u.target_generation != generation_) return Status::kStaleRevision;
    auto it = strokes_.find(u.stroke_id); if (it == strokes_.end()) return Status::kNotFound;
    if (u.truncate_confirmed_to > it->second.confirmed.size()) return Status::kInvalidArgument;
    const auto previousCount = it->second.confirmed.size();
    const bool appendOnly = bitmapInitialized_ && u.truncate_confirmed_to == previousCount &&
                            u.predicted_tail_count == 0U && u.confirmed_append_count != 0U;
    it->second.confirmed.resize(u.truncate_confirmed_to);
    append(it->second.confirmed, u.confirmed_append, u.confirmed_append_count,
           u.confirmed_append_stride);
    it->second.predicted.clear();
    append(it->second.predicted, u.predicted_tail, u.predicted_tail_count,
           u.predicted_tail_stride);
    const bool presented = appendOnly && strokes_.size() == 1U
                               ? renderAppend(it->second, it->second.confirmed, previousCount)
                               : render();
    return presented ? Status::kOk : Status::kPresentationFailed;
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
    if (!attached_) return false;
    // A fully cleared layered window is not sufficient on every Windows
    // compositor path: the last submitted pixels may remain latched while
    // the surface is still visible. Explicitly hide the transient target
    // when Arc has no strokes left; the next Begin/Push path shows it again.
    if (strokes_.empty()) {
      ShowWindow(surface_, SW_HIDE);
      // The layered window may keep the last DIB contents after it is hidden.
      // Mark the cache invalid so the first update of the next stroke performs
      // a full clear/redraw instead of appending onto stale pixels.
      bitmapInitialized_ = false;
      return true;
    }
    if (surface_ == nullptr) {
      if (!RegisterPreviewClass()) return false;
      surface_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
                                      WS_EX_TOOLWINDOW, kClassName, L"Axiom Arc Preview",
                                  WS_POPUP, 0, 0, static_cast<int>(target_.width_pixels),
                                  static_cast<int>(target_.height_pixels), owner_, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
      if (surface_ == nullptr) return false;
    }
    POINT origin{0, 0}; if (!ClientToScreen(owner_, &origin)) return false;
    SetWindowPos(surface_, HWND_TOP, origin.x, origin.y, static_cast<int>(target_.width_pixels),
                 static_cast<int>(target_.height_pixels), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(surface_, SW_SHOWNA);
    HDC screen = GetDC(nullptr);
    if (mem_ == nullptr || bitmap_ == nullptr || bits_ == nullptr) {
      mem_ = CreateCompatibleDC(screen);
      BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = target_.width_pixels;
      bi.bmiHeader.biHeight = -static_cast<LONG>(target_.height_pixels);
      bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
      bitmap_ = CreateDIBSection(mem_, &bi, DIB_RGB_COLORS, &bits_, nullptr, 0);
      if (!mem_ || !bitmap_ || !bits_) {
        if (bitmap_) DeleteObject(bitmap_); bitmap_ = nullptr;
        if (mem_) DeleteDC(mem_); mem_ = nullptr; bits_ = nullptr;
        ReleaseDC(nullptr, screen); return false;
      }
      oldBitmap_ = SelectObject(mem_, bitmap_);
    }
    const size_t count = static_cast<size_t>(target_.width_pixels) * target_.height_pixels;
    std::fill_n(static_cast<std::uint32_t*>(bits_), count, 0u);
    for (const auto& [id, s] : strokes_) { (void)id; draw(mem_, s, s.confirmed); draw(mem_, s, s.predicted); }
    auto* rgba = static_cast<std::uint32_t*>(bits_);
    for (size_t i = 0; i < count; ++i) {
      if ((rgba[i] & 0x00ffffffu) != 0) rgba[i] = (rgba[i] & 0x00ffffffu) | 0x88000000u;
    }
    SIZE size{static_cast<LONG>(target_.width_pixels), static_cast<LONG>(target_.height_pixels)};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    POINT source{0, 0};
    const BOOL ok = UpdateLayeredWindow(surface_, screen, &origin, &size, mem_, &source, 0,
                                        &blend, ULW_ALPHA);
    bitmapInitialized_ = ok != FALSE; ReleaseDC(nullptr, screen); return ok != FALSE;
  }
  bool renderAppend(const Stroke& stroke, const std::vector<arc_preview_primitive_v0>& points,
                    std::size_t previousCount) {
    if (points.empty() || previousCount >= points.size()) return render();
    if (surface_ == nullptr || mem_ == nullptr || bits_ == nullptr) return render();
    HDC screen = GetDC(nullptr);
    POINT origin{0, 0}; if (!ClientToScreen(owner_, &origin)) { ReleaseDC(nullptr, screen); return false; }
    SetWindowPos(surface_, HWND_TOP, origin.x, origin.y, static_cast<int>(target_.width_pixels),
                 static_cast<int>(target_.height_pixels), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(surface_, SW_SHOWNA);
    drawRange(mem_, stroke, points, previousCount > 1U ? previousCount - 2U : 0U);
    const size_t count = static_cast<size_t>(target_.width_pixels) * target_.height_pixels;
    auto* rgba = static_cast<std::uint32_t*>(bits_);
    for (size_t i = 0; i < count; ++i)
      if ((rgba[i] & 0x00ffffffu) != 0) rgba[i] = (rgba[i] & 0x00ffffffu) | 0x88000000u;
    SIZE size{static_cast<LONG>(target_.width_pixels), static_cast<LONG>(target_.height_pixels)};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA}; POINT source{0, 0};
    const BOOL ok = UpdateLayeredWindow(surface_, screen, &origin, &size, mem_, &source, 0,
                                        &blend, ULW_ALPHA);
    bitmapInitialized_ = ok != FALSE; ReleaseDC(nullptr, screen); return ok != FALSE;
  }
  void draw(HDC dc, const Stroke& stroke, const std::vector<arc_preview_primitive_v0>& p) {
    if (p.empty()) return;
    HPEN pen = CreatePen(PS_SOLID, 8, RGB(40, 190, 235)); HGDIOBJ old = SelectObject(dc, pen);
    const auto resource = resources_.find(stroke.resourceId);
    if (resource != resources_.end() && resource->second.width != 0) {
      for (const auto& v : p) {
        if (v.kind != ARC_PREVIEW_PRIMITIVE_DAB) continue;
        const auto& texture = resource->second;
        for (std::uint32_t y = 0; y < texture.height; y += 2U) {
          for (std::uint32_t x = 0; x < texture.width; x += 2U) {
            if (texture.alpha[static_cast<std::size_t>(y) * texture.width + x] < 128U) continue;
            const int px = static_cast<int>(v.x + (static_cast<float>(x) / texture.width - 0.5F) * v.radius * 2.0F);
            const int py = static_cast<int>(v.y + (static_cast<float>(y) / texture.height - 0.5F) * v.radius * 2.0F);
            Ellipse(dc, px - 1, py - 1, px + 2, py + 2);
          }
        }
      }
      SelectObject(dc, old); DeleteObject(pen); return;
    }
    std::vector<POINT> points; points.reserve(p.size());
    for (const auto& v : p) points.push_back({static_cast<LONG>(v.x), static_cast<LONG>(v.y)});
    MoveToEx(dc, points.front().x, points.front().y, nullptr);
    if (points.size() == 2U) {
      LineTo(dc, points.back().x, points.back().y);
    } else {
      for (std::size_t i = 0; i + 1U < points.size(); ++i) {
        const POINT& p0 = points[i == 0U ? i : i - 1U];
        const POINT& p1 = points[i];
        const POINT& p2 = points[i + 1U];
        const POINT& p3 = points[i + 2U < points.size() ? i + 2U : i + 1U];
        POINT controls[3] = {
            {p1.x + (p2.x - p0.x) / 6, p1.y + (p2.y - p0.y) / 6},
            {p2.x - (p3.x - p1.x) / 6, p2.y - (p3.y - p1.y) / 6}, p2};
        PolyBezierTo(dc, controls, 3);
      }
    }
    SelectObject(dc, old); DeleteObject(pen);
  }
  void drawRange(HDC dc, const Stroke& stroke, const std::vector<arc_preview_primitive_v0>& p,
                        std::size_t start) {
    if (p.empty() || start >= p.size()) return;
    std::vector<arc_preview_primitive_v0> tail(p.begin() + static_cast<std::ptrdiff_t>(start), p.end());
    draw(dc, stroke, tail);
  }
  void releaseBitmap() {
    if (mem_ && oldBitmap_) SelectObject(mem_, oldBitmap_);
    if (bitmap_) DeleteObject(bitmap_); bitmap_ = nullptr;
    if (mem_) DeleteDC(mem_); mem_ = nullptr; bits_ = nullptr; oldBitmap_ = nullptr;
    bitmapInitialized_ = false;
  }
  void destroy() {
    if (surface_) DestroyWindow(surface_); surface_ = nullptr;
    releaseBitmap();
    owner_ = nullptr;
    attached_ = false; test_only_ = false; generation_ = 0; bitmapInitialized_ = false; strokes_.clear(); }
  HWND owner_ = nullptr, surface_ = nullptr; arc_preview_target_v0 target_{}; uint64_t generation_ = 0;
  HDC mem_ = nullptr; HBITMAP bitmap_ = nullptr; HGDIOBJ oldBitmap_ = nullptr;
  void* bits_ = nullptr;
  bool bitmapInitialized_ = false;
  bool attached_ = false; bool test_only_ = false; std::unordered_map<uint64_t, Stroke> strokes_;
  std::unordered_map<uint64_t, Resource> resources_;
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
#endif
