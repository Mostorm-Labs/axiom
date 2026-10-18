#include "web_surface_host.hpp"

#include "canvas/render/webgl_surface_backend.hpp"
#include "canvas/input/platform_input_contract.hpp"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

namespace {
struct WebInstance final {
    canvas::render::ViewId view{301};
    std::uint64_t surfaceGeneration = 1;
    std::uint64_t metricsGeneration = 1;
    std::uint64_t pointerSamples = 0;
    std::uint64_t pointerSequence = 0;
    std::uint64_t presentedFrames = 0;
    float logicalWidth = 0.0F;
    float logicalHeight = 0.0F;
    std::uint32_t physicalWidth = 0;
    std::uint32_t physicalHeight = 0;
    float devicePixelRatio = 1.0F;
    canvas::web_demo::WebSurfaceHost lifecycle;
    std::unique_ptr<canvas::render::WebGlSurfaceBackend> backend;
    WebInstance(float lw, float lh, std::uint32_t pw, std::uint32_t ph, float dpr)
        : logicalWidth(lw), logicalHeight(lh), physicalWidth(pw), physicalHeight(ph),
          devicePixelRatio(dpr), lifecycle(canvas::render::SurfaceSnapshot{
              view, canvas::render::SurfaceGeneration{surfaceGeneration},
              canvas::render::MetricsGeneration{metricsGeneration},
              canvas::render::SurfaceMetrics{lw, lh, pw, ph, dpr, 1.0F}}) {}
};
WebInstance* instance(std::uintptr_t handle) { return reinterpret_cast<WebInstance*>(handle); }
}

extern "C" {
EMSCRIPTEN_KEEPALIVE int axiom_web_pointer_sample_phase(
    std::uintptr_t handle, float x, float y, float pressure,
    std::uint32_t pointerId, std::uint32_t phase, std::uint64_t timestampNs);
EMSCRIPTEN_KEEPALIVE std::uintptr_t axiom_web_create(
    const char* selector, float lw, float lh, std::uint32_t pw, std::uint32_t ph, float dpr) {
    if (selector == nullptr || lw <= 0.0F || lh <= 0.0F || pw == 0U || ph == 0U || dpr <= 0.0F) return 0;
    auto value = std::make_unique<WebInstance>(lw, lh, pw, ph, dpr);
    value->backend = std::make_unique<canvas::render::WebGlSurfaceBackend>(
        canvas::render::WebGlSurfaceConfig{selector, pw, ph});
    if (!value->backend->ready()) return 0;
    auto surfaceProvider = canvas::render::WebGlSurfaceProvider::fromCurrentContext(pw, ph);
    if (surfaceProvider == nullptr ||
        !value->lifecycle.composition().registerSurfaceProvider(
            "webgl-window", std::move(surfaceProvider)) ||
        !value->lifecycle.syncRuntimeSurfaceIdentity()) return 0;
    auto previewProvider = std::make_unique<canvas::render::WebGlSurfaceProvider>(
        canvas::render::WebGlSurfaceConfig{"#axiom-preview", pw, ph, 0,
            [](bool visible) { EM_ASM({
                const canvas = document.querySelector('#axiom-preview');
                if (canvas) canvas.style.visibility = $0 ? 'visible' : 'hidden';
            }, visible ? 1 : 0); }});
    if (!previewProvider || !previewProvider->ready() ||
        !value->lifecycle.composition().registerPreviewSurfaceProvider(
            "webgl-preview", std::move(previewProvider))) return 0;
    (void)value->lifecycle.composition().setPlatformPresentationDeferred(true);
    value->surfaceGeneration = value->lifecycle.lifecycle().current().surfaceGeneration.value();
    value->metricsGeneration = value->lifecycle.lifecycle().current().metricsGeneration.value();
    if (!value->lifecycle.composition().registerSource(1U)) return 0;
    return reinterpret_cast<std::uintptr_t>(value.release());
}
EMSCRIPTEN_KEEPALIVE void axiom_web_destroy(std::uintptr_t handle) { delete instance(handle); }
EMSCRIPTEN_KEEPALIVE int axiom_web_resize(
    std::uintptr_t handle, const char* selector, float lw, float lh,
    std::uint32_t pw, std::uint32_t ph, float dpr) {
    auto* value = instance(handle);
    if (value == nullptr || selector == nullptr || lw <= 0.0F || lh <= 0.0F || pw == 0U || ph == 0U || dpr <= 0.0F) return 0;
    const canvas::render::SurfaceMetrics metrics{lw, lh, pw, ph, dpr, 1.0F};
    auto backend = std::make_unique<canvas::render::WebGlSurfaceBackend>(
        canvas::render::WebGlSurfaceConfig{selector, pw, ph});
    if (!backend->ready()) return 0;
    auto surfaceProvider = canvas::render::WebGlSurfaceProvider::fromCurrentContext(pw, ph);
    if (surfaceProvider == nullptr ||
        !value->lifecycle.composition().registerSurfaceProvider(
            std::string("webgl-window-") + std::to_string(value->surfaceGeneration + 1U),
            std::move(surfaceProvider))) return 0;
    auto previewProvider = std::make_unique<canvas::render::WebGlSurfaceProvider>(
        canvas::render::WebGlSurfaceConfig{"#axiom-preview", pw, ph, 0,
            [](bool visible) { EM_ASM({
                const canvas = document.querySelector('#axiom-preview');
                if (canvas) canvas.style.visibility = $0 ? 'visible' : 'hidden';
            }, visible ? 1 : 0); }});
    if (!previewProvider || !previewProvider->ready() ||
        !value->lifecycle.composition().registerPreviewSurfaceProvider(
            "webgl-preview-" + std::to_string(value->surfaceGeneration + 1U),
            std::move(previewProvider))) return 0;
    (void)value->lifecycle.composition().setPlatformPresentationDeferred(true);
    const auto current = value->lifecycle.composition().surfaceSnapshot();
    const auto surface = current.surfaceGeneration;
    const auto metricsGeneration = current.metricsGeneration;
    if (!value->lifecycle.composition().surfaceSnapshot().available ||
        value->lifecycle.bind(canvas::render::SurfaceSnapshot{
            value->view, canvas::render::SurfaceGeneration{surface},
            canvas::render::MetricsGeneration{metricsGeneration}, metrics}) !=
            canvas::web_demo::WebSurfaceDisposition::kRebound) return 0;
    value->surfaceGeneration = surface;
    value->metricsGeneration = metricsGeneration;
    value->logicalWidth = lw;
    value->logicalHeight = lh;
    value->physicalWidth = pw;
    value->physicalHeight = ph;
    value->devicePixelRatio = dpr;
    value->backend = std::move(backend);
    return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_lose_surface(std::uintptr_t handle) {
    auto* value = instance(handle);
    if (value == nullptr) return 0;
    value->backend.reset();
    return value->lifecycle.markLost() == canvas::render::SurfaceLifecycleDisposition::kLost;
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_web_surface_generation(std::uintptr_t handle) {
    const auto* value = instance(handle);
    return value == nullptr ? 0U : value->lifecycle.lifecycle().current().surfaceGeneration.value();
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_web_metrics_generation(std::uintptr_t handle) {
    const auto* value = instance(handle);
    return value == nullptr ? 0U : value->lifecycle.lifecycle().current().metricsGeneration.value();
}
EMSCRIPTEN_KEEPALIVE int axiom_web_backend_ready(std::uintptr_t handle) {
    const auto* value = instance(handle); return value != nullptr && value->backend && value->backend->ready();
}
EMSCRIPTEN_KEEPALIVE int axiom_web_pointer_sample(
    std::uintptr_t handle, float x, float y, float pressure, std::uint32_t) {
    return axiom_web_pointer_sample_phase(handle, x, y, pressure, 1U, 1U, 0U);
}
EMSCRIPTEN_KEEPALIVE int axiom_web_pointer_sample_phase(
    std::uintptr_t handle, float x, float y, float pressure,
    std::uint32_t pointerId, std::uint32_t phase, std::uint64_t timestampNs) {
    auto* value = instance(handle);
    if (value == nullptr || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(pressure) || pressure < 0.0F || pressure > 1.0F) return 0;
    if (phase > static_cast<std::uint32_t>(canvas::input::PointerPhase::kCancel)) return 0;
    const auto sequence = ++value->pointerSequence;
    const auto timestamp = timestampNs == 0U ? sequence * 1'000'000U : timestampNs;
    canvas::input::PlatformPointerBatch batch;
    batch.samples.push_back(canvas::input::PlatformPointerSample{
        1U, pointerId, sequence, timestamp, x, y, pressure, 0.0F, 0.0F,
        {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
        static_cast<canvas::input::PointerPhase>(phase)});
    if (!value->lifecycle.composition().submit(batch)) return 0;
    ++value->pointerSamples;
    return 1;
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_web_pointer_samples(std::uintptr_t handle) {
    const auto* value = instance(handle); return value == nullptr ? 0U : value->pointerSamples;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_present_smoke(std::uintptr_t handle) {
    auto* value = instance(handle);
    if (value == nullptr || !value->backend || !value->backend->ready()) return 0;
    auto& composition = value->lifecycle.composition();
    // The public frame opportunity is the shared Runtime owner safe point.
    // Advance controls/resources before capturing preview state so an idle
    // rAF frame cannot present a stale preview after a queued Debug UI change.
    canvas::runtime::RuntimeFrameOpportunity opportunity{};
    if (!composition.serviceFrameOpportunity(true, opportunity) ||
        !opportunity.canonicalFrame.has_value()) return 0;
    const auto frameId = canvas::render::FrameId{
        opportunity.canonicalFrame->frameId.value()};
    canvas::interaction::PreviewPresentationCapture previewCapture{};
    std::uint64_t previewPresents = 0U;
    const bool havePreview = composition.capturePreviewPresentation(previewCapture) &&
        composition.renderPreviewPresentation(previewCapture, &previewPresents);
    const auto submitted = composition.lastSubmittedCanonicalFrame();
    if (!submitted.has_value() || submitted->frameId != frameId) return 0;
    if (!composition.receivePlatformPresented({
            submitted->viewId, submitted->frameId, submitted->surfaceGeneration,
            submitted->metricsGeneration, canvas::render::PresentOutcome::kPresented,
            canvas::render::PresentEvidenceKind::kPlatformQualified,
            std::nullopt})) return 0;
    if (havePreview && !composition.acknowledgePreviewPresentation(
            previewCapture, previewPresents)) return 0;
    ++value->presentedFrames;
    return 1;
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_web_presented_frames(std::uintptr_t handle) {
    const auto* value = instance(handle); return value == nullptr ? 0U : value->presentedFrames;
}
}
