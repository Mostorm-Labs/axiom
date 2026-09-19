#include "windows_canonical_surface_host.hpp"

#if defined(_WIN32)

#include <windows.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

class SmokeBackend final : public canvas::render::IRenderBackend {
  public:
    canvas::render::BackendSubmissionResult submit(
        const canvas::render::FramePlan&) override {
        return canvas::render::BackendSubmissionResult::accepted();
    }
};

int smokeEvidence(const std::string& path) {
    canvas::windows_demo::WindowsCanonicalSurfaceHost host;
    std::string error;
    if (!host.initialize(800U, 600U, &error) ||
        !host.resizeForSmoke(1024U, 768U, &error) || !host.pumpOnce(&error)) {
        std::ofstream output(path);
        output << "{\"result\":\"FAIL\",\"stage\":\"initialize-or-resize\",\"error\":\""
               << error << "\"}\n";
        return 1;
    }
    const auto& snapshot = host.observation();
    const canvas::render::FrameState frame{
        .viewId = canvas::render::ViewId{1},
        .camera = canvas::render::CameraState{
            canvas::foundation::WorldPoint{0.0F, 0.0F}, 1.0F, 0.0F,
            canvas::render::CameraGeneration{1}},
        .worldViewport = canvas::foundation::WorldRect{-512.0F, -384.0F, 512.0F, 384.0F},
        .metrics = canvas::render::SurfaceMetrics{1024.0F, 768.0F, 1024U, 768U, 1.0F, 1.0F},
        .sceneGeneration = canvas::semantic::SemanticGeneration{1},
        .sceneReadToken = canvas::foundation::SceneRevision{1},
        .surfaceGeneration = canvas::render::SurfaceGeneration{snapshot.surfaceGeneration},
        .metricsGeneration = canvas::render::MetricsGeneration{snapshot.metricsGeneration},
        .frameId = canvas::render::FrameId{1}};
    canvas::render::ReferenceDrawList drawList{};
    drawList.frame = frame;
    const auto plan = canvas::render::FramePlanBuilder::build(frame, drawList);
    SmokeBackend backend;
    if (!plan || !host.submit(backend, plan.value(), &error)) {
        std::ofstream output(path);
        output << "{\"result\":\"FAIL\",\"stage\":\"submit\",\"error\":\""
               << error << "\"}\n";
        return 1;
    }
    const auto& observation = host.observation();
    std::ofstream output(path);
    output << "{\"result\":\"PASS\",\"platform\":\"windows-2025\","
           << "\"target\":\"g3-windows-release\",\"host\":\"axiom_canvas_demo_windows\","
           << "\"resize_events\":" << observation.resizeEvents
           << ",\"surface_generation\":" << observation.surfaceGeneration
           << ",\"metrics_generation\":" << observation.metricsGeneration
           << ",\"submitted_frames\":" << observation.submittedFrames
           << ",\"presented_frames\":" << observation.presentedFrames
           << ",\"present_feedback\":\"platform-qualified\",\"gdi_live_present_used\":false}\n";
    return output.good() ? 0 : 1;
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int) {
    const std::string command = commandLine == nullptr ? "" : commandLine;
    constexpr std::string_view prefix = "--smoke-evidence=";
    if (command.starts_with(prefix)) return smokeEvidence(command.substr(prefix.size()));
    canvas::windows_demo::WindowsCanonicalSurfaceHost host;
    std::string error;
    if (!host.initialize(800U, 600U, &error)) {
        std::cerr << error << '\n';
        return 1;
    }
    for (int frame = 0; frame < 3; ++frame) {
        if (!host.pumpOnce(&error)) return 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    return 0;
}

#else
int main() { return 2; }
#endif
