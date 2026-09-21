#include "brush_lab.hpp"
#include "reference_brushes.hpp"
#if defined(CANVAS_RENDER_HAS_SKIA)
#include "canvas/render/skia_ink_backend.hpp"
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <span>
#include <algorithm>
#include <memory>
#include <chrono>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#endif

namespace {

#if defined(_WIN32)
using canvas::brush_lab::ReferenceBrushId;
using canvas::brush_lab::ReferenceBrushPreset;
using canvas::brush_lab::ReferenceBrushSet;
using canvas::ink::BrushInputSample;
using canvas::ink::BrushPrimitive;

struct GuiState final {
  HWND window = nullptr;
  ReferenceBrushSet brushes = canvas::brush_lab::makeReferenceBrushSet();
  std::size_t selected = 0;
  bool dualColor = false;
  bool editing = false;
  std::uint64_t sequence = 0;
  std::uint64_t session = 0;
  std::vector<BrushInputSample> input;
  std::vector<BrushInputSample> pendingSamples;
  std::vector<BrushPrimitive> preview;
  std::vector<std::vector<BrushPrimitive>> committed;
  std::vector<std::uint64_t> digests;
  std::unique_ptr<canvas::brush_lab::ReferenceBrushStrokeSession> activeSession;
  bool drawing = false;
  bool pointerDrawing = false;
  HDC backDc = nullptr;
  HBITMAP backBitmap = nullptr;
  HBITMAP previousBitmap = nullptr;
  int backWidth = 0;
  int backHeight = 0;
  std::chrono::steady_clock::time_point lastFlush{};
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::SkiaInkBackend> renderer;
#endif
};

GuiState* getState(HWND window) {
  return reinterpret_cast<GuiState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

const ReferenceBrushPreset& selectedBrush(const GuiState& state) {
  return state.brushes.presets[state.selected];
}

bool flushPendingSamples(GuiState& state) {
  if (state.pendingSamples.empty() || state.activeSession == nullptr) return false;
  const auto result = state.activeSession->append(state.pendingSamples);
  state.preview = result.preview.primitives;
  state.pendingSamples.clear();
  state.lastFlush = std::chrono::steady_clock::now();
  return true;
}

bool appendSample(GuiState& state, float x, float y) {
  if (state.input.empty()) {
    ++state.session;
    state.sequence = 0;
    state.input.clear();
    state.preview.clear();
    state.pendingSamples.clear();
    state.lastFlush = {};
    state.activeSession = std::make_unique<canvas::brush_lab::ReferenceBrushStrokeSession>(
        selectedBrush(state), state.brushes.resources, state.session);
  }
  const auto sample = BrushInputSample{x, y, 1.0F, 0.0F, 0.0F, ++state.sequence};
  state.input.push_back(sample);
  state.pendingSamples.push_back(sample);
  const auto now = std::chrono::steady_clock::now();
  const bool firstSample = state.input.size() == 1U;
  const bool frameDue = state.lastFlush.time_since_epoch().count() == 0 ||
      now - state.lastFlush >= std::chrono::milliseconds(8);
  return (firstSample || frameDue) ? flushPendingSamples(state) : false;
}

void finishStroke(GuiState& state) {
  if (state.input.empty()) return;
  (void)flushPendingSamples(state);
  const auto result = state.activeSession != nullptr
      ? state.activeSession->finish() : canvas::ink::BrushRuntimeResult{};
  if (!result.commit.primitives.empty()) {
    state.committed.push_back(result.commit.primitives);
    state.digests.push_back(result.commit.digest);
  }
  state.input.clear();
  state.pendingSamples.clear();
  state.preview.clear();
  state.sequence = 0;
  state.activeSession.reset();
}

void drawDabs(HDC dc, const std::vector<BrushPrimitive>& dabs, COLORREF color,
              bool textured) {
  HBRUSH brush = CreateSolidBrush(color);
  HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
  HGDIOBJ oldBrush = SelectObject(dc, brush);
  HGDIOBJ oldPen = SelectObject(dc, pen);
  for (const auto& dab : dabs) {
    const int radius = (std::max)(1, static_cast<int>(dab.size * 0.5F));
    const int left = static_cast<int>(dab.x) - radius;
    const int top = static_cast<int>(dab.y) - radius;
    Ellipse(dc, left, top, left + radius * 2, top + radius * 2);
    if (textured && radius > 3) {
      MoveToEx(dc, static_cast<int>(dab.x) - radius / 2,
               static_cast<int>(dab.y), nullptr);
      LineTo(dc, static_cast<int>(dab.x) + radius / 2,
             static_cast<int>(dab.y));
    }
  }
  SelectObject(dc, oldPen);
  SelectObject(dc, oldBrush);
  DeleteObject(pen);
  DeleteObject(brush);
}

#if defined(CANVAS_RENDER_HAS_SKIA)
bool renderCanonicalWithSkia(HWND window, GuiState& state) {
  (void)window;
  if (state.renderer == nullptr || (state.committed.empty() && state.preview.empty())) return false;
  if (state.renderer->resize(static_cast<std::uint32_t>(state.backWidth),
                             static_cast<std::uint32_t>(state.backHeight)).code !=
      canvas::render::BackendSubmissionCode::kAccepted) return false;
  std::vector<std::shared_ptr<const canvas::ink::BrushRenderResource>> resources;
  std::vector<canvas::render::ProgrammableDab> dabs;
  const auto appendDabs = [&](const std::vector<BrushPrimitive>& stroke,
                              std::uint32_t color) {
    for (const auto& primitive : stroke) {
      const auto resource = state.brushes.resources.renderResource(
          primitive.shapeResource, primitive.grainResource);
      if (!resource) continue;
      resources.push_back(resource);
      dabs.push_back({primitive.x, primitive.y, primitive.size,
                      primitive.rotation * 57.2957795F, primitive.opacity,
                      resource->width, resource->height, resource->alpha, color});
    }
  };
  for (const auto& stroke : state.committed) {
    appendDabs(stroke, state.dualColor ? 0xffef3d74U : 0xff1a5bffU);
  }
  appendDabs(state.preview, state.dualColor ? 0xff19c7e8U : 0xff28bee6U);
  if (dabs.empty()) return false;
  if (state.renderer->submitProgrammableDabs(dabs).code !=
      canvas::render::BackendSubmissionCode::kAccepted) return false;
  const auto pixels = state.renderer->rgba();
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = state.backWidth;
  info.bmiHeader.biHeight = -state.backHeight;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  SetDIBitsToDevice(state.backDc, 0, 0, static_cast<DWORD>(state.backWidth),
                    static_cast<DWORD>(state.backHeight), 0, 0,
                    0, static_cast<UINT>(state.backHeight), pixels.data(), &info,
                    DIB_RGB_COLORS);
  return true;
}
#endif

void renderBackbuffer(HWND window, GuiState& state) {
  HDC windowDc = GetDC(window);
  RECT rect{};
  GetClientRect(window, &rect);
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  if (width <= 0 || height <= 0) {
    ReleaseDC(window, windowDc);
    return;
  }
  if (state.backDc == nullptr || state.backWidth != width || state.backHeight != height) {
    if (state.backBitmap != nullptr) DeleteObject(state.backBitmap);
    if (state.backDc != nullptr) DeleteDC(state.backDc);
    state.backDc = CreateCompatibleDC(windowDc);
    state.backBitmap = CreateCompatibleBitmap(windowDc, width, height);
    state.previousBitmap = static_cast<HBITMAP>(SelectObject(state.backDc, state.backBitmap));
    state.backWidth = width;
    state.backHeight = height;
  }
  HBRUSH background = CreateSolidBrush(RGB(250, 250, 252));
  FillRect(state.backDc, &rect, background);
  DeleteObject(background);
#if defined(CANVAS_RENDER_HAS_SKIA)
  const bool skiaRendered = renderCanonicalWithSkia(window, state);
#else
  const bool skiaRendered = false;
#endif
  SetBkMode(state.backDc, TRANSPARENT);
  SetTextColor(state.backDc, RGB(35, 35, 45));
  std::wstring title = L"Axiom Reference Brush Lab | 1-5 select | Space dual-color | ";
  for (const char* name = selectedBrush(state).name; *name != '\0'; ++name)
    title.push_back(static_cast<wchar_t>(*name));
  if (state.editing) title += L" | RB-03 edited";
  TextOutW(state.backDc, 16, 14, title.c_str(), static_cast<int>(title.size()));
  if (!skiaRendered) for (const auto& stroke : state.committed) {
    drawDabs(state.backDc, stroke,
             state.dualColor ? RGB(25, 55, 150) : RGB(20, 70, 210), true);
  }
  if (!skiaRendered && !state.preview.empty()) {
    drawDabs(state.backDc, state.preview,
             state.dualColor ? RGB(80, 210, 245) : RGB(100, 170, 245), true);
  }
  std::wstring help = L"Mouse/pen: draw   1 Fine Ink  2 Pressure Marker  3 Dry Chalk  4 Soft Airbrush  5 Decorative Broad   E: edit RB-03   R: restore";
  TextOutW(state.backDc, 16, height - 28, help.c_str(), static_cast<int>(help.size()));
  ReleaseDC(window, windowDc);
}

void paint(HWND window, GuiState& state) {
  PAINTSTRUCT ps{};
  HDC dc = BeginPaint(window, &ps);
  renderBackbuffer(window, state);
  if (state.backDc != nullptr) {
    BitBlt(dc, 0, 0, state.backWidth, state.backHeight, state.backDc, 0, 0, SRCCOPY);
  }
  EndPaint(window, &ps);
}

void requestRepaint(HWND window, GuiState& state) {
  (void)state;
  InvalidateRect(window, nullptr, FALSE);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* state = getState(window);
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    return TRUE;
  }
  if (state == nullptr) return DefWindowProcW(window, message, wParam, lParam);
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE || message == WM_POINTERUP) {
    POINTER_INPUT_TYPE type{};
    const UINT pointerId = GET_POINTERID_WPARAM(wParam);
    if (GetPointerType(pointerId, &type) && type == PT_MOUSE) return 0;
  }
  switch (message) {
    case WM_LBUTTONDOWN:
      if (state->pointerDrawing) return 0;
      state->drawing = true;
      SetCapture(window);
      if (appendSample(*state, static_cast<float>(GET_X_LPARAM(lParam)),
                       static_cast<float>(GET_Y_LPARAM(lParam))))
        requestRepaint(window, *state);
      return 0;
    case WM_POINTERDOWN:
      state->pointerDrawing = true;
      state->drawing = true;
      SetCapture(window);
      if (appendSample(*state, static_cast<float>(GET_X_LPARAM(lParam)),
                       static_cast<float>(GET_Y_LPARAM(lParam))))
        requestRepaint(window, *state);
      return 0;
    case WM_POINTERUPDATE:
      if (state->pointerDrawing && state->drawing) {
        if (appendSample(*state, static_cast<float>(GET_X_LPARAM(lParam)),
                         static_cast<float>(GET_Y_LPARAM(lParam))))
          requestRepaint(window, *state);
      }
      return 0;
    case WM_MOUSEMOVE:
      if (!state->pointerDrawing && state->drawing && (wParam & MK_LBUTTON)) {
        if (appendSample(*state, static_cast<float>(GET_X_LPARAM(lParam)),
                         static_cast<float>(GET_Y_LPARAM(lParam))))
          requestRepaint(window, *state);
      }
      return 0;
    case WM_LBUTTONUP:
      if (state->pointerDrawing) return 0;
      state->drawing = false;
      finishStroke(*state);
      ReleaseCapture();
      requestRepaint(window, *state);
      return 0;
    case WM_POINTERUP:
      state->drawing = false;
      state->pointerDrawing = false;
      finishStroke(*state);
      ReleaseCapture();
      requestRepaint(window, *state);
      return 0;
    case WM_KEYDOWN:
      if (wParam == VK_SPACE) state->dualColor = !state->dualColor;
      else if (wParam >= '1' && wParam <= '5') state->selected = static_cast<std::size_t>(wParam - '1');
      else if (wParam == 'E' && state->selected == 2) {
        auto* preset = const_cast<ReferenceBrushPreset*>(state->brushes.find(ReferenceBrushId::kDryChalk));
        if (preset != nullptr) *preset = canvas::brush_lab::editDryChalk(*preset, {19.0F, 0.34F, 0.62F});
        state->editing = true;
      } else if (wParam == 'R' && state->selected == 2) {
        state->brushes = canvas::brush_lab::makeReferenceBrushSet();
        state->editing = false;
      }
      requestRepaint(window, *state);
      return 0;
    case WM_PAINT:
      paint(window, *state);
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      requestRepaint(window, *state);
      return 0;
    case WM_DESTROY:
      if (state->backBitmap != nullptr) DeleteObject(state->backBitmap);
      if (state->backDc != nullptr) DeleteDC(state->backDc);
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(window, message, wParam, lParam);
  }
}

int runGui() {
  HINSTANCE instance = GetModuleHandleW(nullptr);
  WNDCLASSW klass{};
  klass.hInstance = instance;
  klass.lpfnWndProc = windowProc;
  klass.lpszClassName = L"AxiomReferenceBrushLab";
  klass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  if (RegisterClassW(&klass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
  GuiState state;
#if defined(CANVAS_RENDER_HAS_SKIA)
  state.renderer = std::make_unique<canvas::render::SkiaInkBackend>();
#endif
  HWND window = CreateWindowExW(0, klass.lpszClassName, L"Axiom Reference Brush Lab",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                1200, 800, nullptr, nullptr, instance, &state);
  if (window == nullptr) return 1;
  state.window = window;
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return static_cast<int>(message.wParam);
}
#endif

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
  if (argc == 2 && std::string(argv[1]) == "--gui") return runGui();
#endif
  if (argc == 1) {
    std::cout << canvas::brush_lab::manifestJson() << '\n';
    return 0;
  }
  if (argc != 2) {
    std::cerr << "usage: axiom_brush_lab [output-directory]\n";
    return 2;
  }
  const std::filesystem::path output(argv[1]);
  std::filesystem::create_directories(output);
  std::ofstream manifest(output / "manifest.json", std::ios::binary);
  manifest << canvas::brush_lab::manifestJson() << '\n';
  std::ofstream referenceManifest(output / "reference-brush-manifest.json", std::ios::binary);
  referenceManifest << canvas::brush_lab::referenceBrushManifestJson() << '\n';
  for (const auto family : {canvas::ink::BrushFamily::kPen,
                            canvas::ink::BrushFamily::kPencil,
                            canvas::ink::BrushFamily::kChalk,
                            canvas::ink::BrushFamily::kMarker,
                            canvas::ink::BrushFamily::kWaterColorLite,
                            canvas::ink::BrushFamily::kHighlighter,
                            canvas::ink::BrushFamily::kLaser}) {
    const auto scenario = canvas::brush_lab::evaluate(family);
    std::ofstream svg(output / (scenario.name + ".svg"), std::ios::binary);
    svg << scenario.svg << '\n';
  }
  return manifest && referenceManifest && !manifest.bad() && !referenceManifest.bad() ? 0 : 1;
}
