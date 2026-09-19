#include "web_surface_host.hpp"

#include "canvas/render/webgl_surface_backend.hpp"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <cmath>
#include <memory>
#include <optional>

namespace {
struct WebInstance final {
    canvas::render::ViewId view{1};
    std::uint64_t surfaceGeneration = 1;
    std::uint64_t metricsGeneration = 1;
    std::uint64_t pointerSamples = 0;
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
EMSCRIPTEN_KEEPALIVE std::uintptr_t axiom_web_create(
    const char* selector, float lw, float lh, std::uint32_t pw, std::uint32_t ph, float dpr) {
    if (selector == nullptr || lw <= 0.0F || lh <= 0.0F || pw == 0U || ph == 0U || dpr <= 0.0F) return 0;
    auto value = std::make_unique<WebInstance>(lw, lh, pw, ph, dpr);
    value->backend = std::make_unique<canvas::render::WebGlSurfaceBackend>(
        canvas::render::WebGlSurfaceConfig{selector, pw, ph});
    if (!value->backend->ready()) return 0;
    return reinterpret_cast<std::uintptr_t>(value.release());
}
EMSCRIPTEN_KEEPALIVE void axiom_web_destroy(std::uintptr_t handle) { delete instance(handle); }
EMSCRIPTEN_KEEPALIVE int axiom_web_resize(
    std::uintptr_t handle, const char* selector, float lw, float lh,
    std::uint32_t pw, std::uint32_t ph, float dpr) {
    auto* value = instance(handle);
    if (value == nullptr || selector == nullptr || lw <= 0.0F || lh <= 0.0F || pw == 0U || ph == 0U || dpr <= 0.0F) return 0;
    const auto surface = value->surfaceGeneration + 1U;
    const auto metricsGeneration = value->metricsGeneration + 1U;
    const canvas::render::SurfaceMetrics metrics{lw, lh, pw, ph, dpr, 1.0F};
    if (value->lifecycle.bind(canvas::render::SurfaceSnapshot{
            value->view, canvas::render::SurfaceGeneration{surface},
            canvas::render::MetricsGeneration{metricsGeneration}, metrics}) !=
        canvas::web_demo::WebSurfaceDisposition::kRebound) return 0;
    auto backend = std::make_unique<canvas::render::WebGlSurfaceBackend>(
        canvas::render::WebGlSurfaceConfig{selector, pw, ph});
    if (!backend->ready()) return 0;
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
    const auto* value = instance(handle); return value == nullptr ? 0U : value->surfaceGeneration;
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_web_metrics_generation(std::uintptr_t handle) {
    const auto* value = instance(handle); return value == nullptr ? 0U : value->metricsGeneration;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_backend_ready(std::uintptr_t handle) {
    const auto* value = instance(handle); return value != nullptr && value->backend && value->backend->ready();
}
EMSCRIPTEN_KEEPALIVE int axiom_web_pointer_sample(
    std::uintptr_t handle, float x, float y, float pressure, std::uint32_t) {
    auto* value = instance(handle);
    if (value == nullptr || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(pressure) || pressure < 0.0F || pressure > 1.0F) return 0;
    ++value->pointerSamples;
    return 1;
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_web_pointer_samples(std::uintptr_t handle) {
    const auto* value = instance(handle); return value == nullptr ? 0U : value->pointerSamples;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_present_smoke(std::uintptr_t handle) {
    auto* value = instance(handle);
    if (value == nullptr || !value->backend || !value->backend->ready()) return 0;
    const auto frameId = canvas::render::FrameId{value->presentedFrames + 1U};
    const canvas::render::FrameState frame{
        .viewId = value->view,
        .camera = canvas::render::CameraState{
            canvas::foundation::WorldPoint{0.0F, 0.0F}, 1.0F, 0.0F,
            canvas::render::CameraGeneration{1}},
        .worldViewport = canvas::foundation::WorldRect{
            0.0F, 0.0F, value->logicalWidth, value->logicalHeight},
        .metrics = canvas::render::SurfaceMetrics{
            value->logicalWidth, value->logicalHeight, value->physicalWidth,
            value->physicalHeight, value->devicePixelRatio, 1.0F},
        .sceneGeneration = canvas::semantic::SemanticGeneration{1},
        .sceneReadToken = canvas::foundation::SceneRevision{1},
        .surfaceGeneration = canvas::render::SurfaceGeneration{value->surfaceGeneration},
        .metricsGeneration = canvas::render::MetricsGeneration{value->metricsGeneration},
        .frameId = frameId,
    };
    if (value->lifecycle.submit(frame) != canvas::render::PresentFeedbackDisposition::kSubmitted) return 0;
    if (value->lifecycle.feedback(canvas::render::PresentedFeedback{
            value->view, frameId,
            canvas::render::SurfaceGeneration{value->surfaceGeneration},
            canvas::render::MetricsGeneration{value->metricsGeneration},
            canvas::render::PresentOutcome::kPresented,
            canvas::render::PresentEvidenceKind::kPlatformQualified,
            std::nullopt}) != canvas::render::PresentFeedbackDisposition::kPresented) return 0;
    ++value->presentedFrames;
    return 1;
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_web_presented_frames(std::uintptr_t handle) {
    const auto* value = instance(handle); return value == nullptr ? 0U : value->presentedFrames;
}
}
