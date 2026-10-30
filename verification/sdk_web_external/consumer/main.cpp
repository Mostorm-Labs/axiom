#include <canvas/platform/web_runtime_adapter.hpp>
#include <canvas/runtime/runtime_composition.hpp>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace {
using canvas::platform::WebFrameResult;
std::unique_ptr<canvas::runtime::RuntimeComposition> owner;
std::unique_ptr<canvas::platform::WebRuntimeAdapter> adapter;
std::unordered_map<std::uint64_t, canvas::input::PointerKey> contacts;
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE canonical = 0, preview = 0;
std::uint64_t sequence = 0;
bool demand = true, force = true, pending = false, pendingPreview = false;
bool previewCompleted = false, upSeen = false, recoveryStarted = false, restoring = false;
std::uint64_t lostGeneration = 0;
bool previewVisibleObserved = false;
bool interactive = false;
bool interactiveReady = false;

void status(const char* value) {
  EM_ASM({ document.getElementById('result').textContent = UTF8ToString($0); }, value);
}
void fail(const char* value) { status(value); emscripten_cancel_main_loop(); }

void tick() {
  if (pending) {
    const auto result = adapter->pollFrame();
    if (result == WebFrameResult::kPending) return;
    if (result != WebFrameResult::kIdle) { fail("FAIL: GPU completion"); return; }
    pending = false;
    if (pendingPreview) {
      previewCompleted = true;
      EM_ASM({ document.getElementById('result').dataset.preview = 'complete'; });
    }
    pendingPreview = false;
  }
  if (demand) {
    const auto result = adapter->prepareFrame(force);
    force = false;
    if (result == WebFrameResult::kRejected) { fail("FAIL: frame request"); return; }
    if (result == WebFrameResult::kRetry) return;
    demand = false;
    if (result == WebFrameResult::kPending) {
      const auto frame = adapter->frameStatus();
      if (!frame.pending) { fail("FAIL: missing work identity"); return; }
      pendingPreview = frame.previewWorkId != 0;
      pending = true;
      return;
    }
  }
  if (interactive) {
    if (!interactiveReady) { status("READY"); interactiveReady = true; }
    return;
  }
  if (!upSeen && !recoveryStarted) { status("READY"); return; }
  if (upSeen && !recoveryStarted) {
    const auto observed = owner->hostObservation();
    if (!previewCompleted || !previewVisibleObserved || !observed || observed->objectCount != 1 || adapter->presentedCount() < 2) {
      fail("FAIL: input or preview not observed"); return;
    }
    EM_ASM({ document.getElementById('result').dataset.commit = 'complete'; });
    if (!EM_ASM_INT({ return document.getElementById('result').dataset.pixelChecked === 'yes' ? 1 : 0; })) return;
    if (restoring) {
      if (!EM_ASM_INT({ return document.getElementById('result').dataset.restored === '2' ? 1 : 0; })) return;
      if (!adapter->bind(canonical, preview, 160, 120, 320, 240, 2.0F)) {
        fail("FAIL: restored WebGL binding"); return;
      }
      const auto rebound = owner->surfaceMetrics();
      if (!rebound || !rebound->available || rebound->surfaceGeneration <= lostGeneration) {
        fail("FAIL: stale surface identity"); return;
      }
      restoring = false; recoveryStarted = true; demand = true; force = true; return;
    }
    if (emscripten_set_canvas_element_size("#surface", 320, 240) != EMSCRIPTEN_RESULT_SUCCESS ||
        emscripten_set_canvas_element_size("#preview", 320, 240) != EMSCRIPTEN_RESULT_SUCCESS ||
        !adapter->bind(canonical, preview, 160, 120, 320, 240, 2.0F)) {
      fail("FAIL: resize"); return;
    }
    const auto m = owner->surfaceMetrics();
    if (!m || !m->available || m->width != 320 || m->height != 240 ||
        m->logicalWidth != 160 || m->logicalHeight != 120 || m->devicePixelRatio != 2 ||
        adapter->prepareFrame(true) != WebFrameResult::kPending ||
        !adapter->frameStatus().canonicalWorkId || !adapter->loseSurface() ||
        adapter->frameStatus().pending || adapter->pollFrame() != WebFrameResult::kIdle) {
      fail("FAIL: loss/recovery"); return;
    }
    lostGeneration = m->surfaceGeneration;
    const int supported = EM_ASM_INT({
      const canvases = [document.getElementById('surface')].concat([document.getElementById('preview')]);
      const extensions = canvases.map(c => c.getContext('webgl2').getExtension('WEBGL_lose_context'));
      if (extensions.some(e => !e)) return 0;
      const result = document.getElementById('result');
      let lost = 0; let restored = 0;
      for (const c of canvases) {
        c.addEventListener('webglcontextlost', e => { e.preventDefault(); result.dataset.lost = String(++lost);
          if (lost === 2) setTimeout(() => extensions.forEach(x => x.restoreContext()), 100); }, {once:true});
        c.addEventListener('webglcontextrestored', () => { result.dataset.restored = String(++restored); }, {once:true});
      }
      extensions.forEach(x => x.loseContext()); return 1;
    });
    if (!supported) { fail("BLOCKED: WEBGL_lose_context unavailable"); return; }
    restoring = true; return;
  }
  if (recoveryStarted) {
    auto& facade = owner->controlFacade();
    const auto targets = facade.mountedCanvasTargets();
    if (targets.empty()) { fail("FAIL: text target"); return; }
    canvas::runtime::TextIntent intent{};
    intent.target = targets.front(); intent.clientId = 702; intent.requestId = 1;
    intent.kind = canvas::runtime::TextIntentKind::kCreate; intent.viewX = 40; intent.viewY = 40;
    if (facade.submitTextIntent(intent).status != canvas::runtime::TextStatus::kQueued || !owner->ownerPump()) {
      fail("FAIL: text create"); return;
    }
    const auto editor = facade.readTextEditor();
    canvas::runtime::TextInputEvent input{};
    input.token = editor.token; input.sequence = editor.nextInputSequence;
    input.kind = canvas::runtime::TextInputKind::kReplace; input.text = u"你好 Axiom";
    if (facade.submitTextInput(input).status != canvas::runtime::TextStatus::kQueued || !owner->ownerPump() ||
        facade.readTextEditor().projection != u"你好 Axiom" || !facade.readTextGeometry().available) {
      fail("FAIL: text input"); return;
    }
    if (!adapter->shutdown() || !owner->close()) { fail("FAIL: shutdown"); return; }
    adapter.reset(); owner.reset();
    emscripten_webgl_destroy_context(preview); emscripten_webgl_destroy_context(canonical);
    status("PASS"); emscripten_cancel_main_loop();
  }
}
}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE float sdk_logical_width() {
  const auto metrics = owner ? owner->surfaceMetrics() : std::nullopt;
  return metrics && metrics->available ? metrics->logicalWidth : 0.0F;
}
extern "C" EMSCRIPTEN_KEEPALIVE void sdk_schedule() { demand = true; }
extern "C" EMSCRIPTEN_KEEPALIVE int sdk_object_count() {
  const auto observation = owner ? owner->hostObservation() : std::nullopt;
  return observation ? static_cast<int>(observation->objectCount) : -1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int sdk_ink_tool() {
  if (!owner) return 0;
  auto& facade = owner->controlFacade();
  const auto targets = facade.mountedCanvasTargets();
  if (targets.empty()) return 0;
  canvas::runtime::CanvasControlRequest request{};
  request.target = targets.front(); request.clientId = 703; request.requestId = ++sequence;
  request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectTool;
  request.payload.tool = canvas::runtime::CanvasToolKind::kInk;
  if (facade.submitCanvasControl(request).state != canvas::runtime::ProductControlState::kQueued) return 0;
  if (!owner->ownerPump()) return 0;
  const auto receipt = facade.canvasControlReceipt({request.clientId, request.requestId});
  return receipt && receipt->state == canvas::runtime::CanvasControlReceiptState::kApplied ? 1 : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE float sdk_logical_height() {
  const auto metrics = owner ? owner->surfaceMetrics() : std::nullopt;
  return metrics && metrics->available ? metrics->logicalHeight : 0.0F;
}
extern "C" EMSCRIPTEN_KEEPALIVE int sdk_input(int source, int pointer, int phase, float x, float y,
    double timestamp, float pressure, int pressureAvailable, float width, float height,
    int geometryAvailable, int historical, int shift) {
  if (!owner || source < 1 || source > 3 || pointer < 1 || phase < 0 || phase > 3 || timestamp <= 0) return 0;
  canvas::input::PlatformPointerSample sample{};
  sample.source = static_cast<std::uint64_t>(source); sample.pointer = static_cast<std::uint64_t>(pointer);
  sample.sequence = ++sequence; sample.timestampNs = static_cast<std::uint64_t>(timestamp);
  sample.x = x; sample.y = y;
  sample.pressure = pressure; sample.capabilities.pressure = pressureAvailable != 0;
  sample.capabilities.tool = source == 3 ? canvas::input::PointerTool::kFinger :
      source == 2 ? canvas::input::PointerTool::kPen : canvas::input::PointerTool::kUnknown;
  sample.contact = {width, height, 0, geometryAvailable != 0};
  sample.capabilities.contactGeometry = geometryAvailable != 0;
  sample.capabilities.coalescedHistory = historical != 0;
  sample.provenance = historical ? canvas::input::SampleProvenance::kCoalescedHistory :
      canvas::input::SampleProvenance::kConfirmedCurrent;
  sample.modifiers = shift ? canvas::input::PointerModifiers::kShift : canvas::input::PointerModifiers::kNone;
  sample.phase = phase == 0 ? canvas::input::PointerPhase::kDown
      : phase == 1 ? canvas::input::PointerPhase::kMove : phase == 2 ? canvas::input::PointerPhase::kUp : canvas::input::PointerPhase::kCancel;
  canvas::input::PlatformPointerBatch batch; batch.samples.push_back(sample);
  const auto identity = (static_cast<std::uint64_t>(source) << 32U) | static_cast<std::uint32_t>(pointer);
  bool accepted = false;
  if (phase == 0) { if (auto contact = owner->beginContact(batch)) { contacts[identity] = *contact; accepted = true; } }
  else if (const auto it = contacts.find(identity); it != contacts.end()) accepted = owner->submitContact(it->second, batch);
  if (phase == 2 || phase == 3) { if (phase == 2) upSeen = accepted; contacts.erase(identity); }
  if (accepted) demand = true;
  return accepted ? 1 : 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int sdk_shutdown() {
  if (!owner) return 1;
  if (!contacts.empty() || !adapter->shutdown() || !owner->close()) return 0;
  adapter.reset(); owner.reset();
  emscripten_webgl_destroy_context(preview); emscripten_webgl_destroy_context(canonical);
  emscripten_cancel_main_loop();
  return 1;
}

// This application-owned bridge projects the public Facade into the browser's
// native text carrier. It does not access any private Runtime implementation.
using namespace canvas;
namespace {
struct ExternalLifecycle final {
  runtime::RuntimeComposition& composition() const { return *owner; }
  runtime::RuntimeFacade& facade() const { return owner->controlFacade(); }
};
struct WebInstance final { ExternalLifecycle lifecycle; std::uint64_t request = 0, lastTextIntent = 0; };
WebInstance externalInstance;
WebInstance* instance(std::uintptr_t handle) { return handle == 1 && owner ? &externalInstance : nullptr; }
}
#include "text_bridge.inl"

int main() {
  interactive = EM_ASM_INT({ return new URLSearchParams(location.search).has('manual') ? 1 : 0; }) != 0;
  EmscriptenWebGLContextAttributes attributes;
  emscripten_webgl_init_context_attributes(&attributes);
  attributes.majorVersion = 2; attributes.stencil = true; attributes.alpha = true;
  attributes.antialias = false; attributes.preserveDrawingBuffer = true;
  canonical = emscripten_webgl_create_context("#surface", &attributes);
  preview = emscripten_webgl_create_context("#preview", &attributes);
  if (canonical <= 0 || preview <= 0) { status("BLOCKED: WebGL2 contexts"); return 1; }
  owner = std::make_unique<canvas::runtime::RuntimeComposition>(701);
  if (!owner->open()) { status("FAIL: owner open"); return 1; }
  adapter = std::make_unique<canvas::platform::WebRuntimeAdapter>(*owner,
      [](void*, bool visible) { if (visible) previewVisibleObserved = true; });
  if (adapter->bind(canonical, canonical, 200, 150, 400, 300, 2.0F) ||
      adapter->bind(canonical, preview, 200, 150, 399, 300, 2.0F)) {
    status("FAIL: invalid binding accepted"); return 1;
  }
  if (!adapter->bind(canonical, preview, 200, 150, 400, 300, 2.0F) || !owner->registerSource(1) ||
      !owner->registerSource(2) || !owner->registerSource(3)) {
    status("FAIL: initial binding"); return 1;
  }
  if (adapter->loadHostFonts("/missing-font.otf", "/fonts") ||
      !adapter->loadHostFonts("/axiom-font.otf", "/fonts") ||
      owner->controlFacade().readFontCatalog().entries.empty() ||
      adapter->loadHostFonts("/axiom-font.otf", "/fonts")) {
    status("FAIL: application font catalog"); return 1;
  }
  const auto fonts = owner->controlFacade().readFontCatalog();
  if (!owner->resourceToken(fonts.entries.front().id)) {
    status("FAIL: public resource token"); return 1;
  }
  emscripten_set_main_loop(tick, 0, 0);
  return 0;
}
