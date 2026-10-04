#include "canvas/debug_ui/controller.hpp"
#include "imgui.h"
#if defined(CANVAS_DEBUG_UI_HAS_SKIA)
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkSurface.h"
#include "include/core/SkVertices.h"
#include "include/effects/SkImageFilters.h"
#include "include/core/SkSamplingOptions.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#endif
namespace canvas::debug_ui {
std::array<PanelState, static_cast<std::size_t>(DebugPanel::kCount)> DebugController::panels() const {
    return {{{DebugPanel::kOverview, true, "Overview"}, {DebugPanel::kInput, true, "Input"},
             {DebugPanel::kCanvas, true, "Canvas"}, {DebugPanel::kArcPreview, true, "Arc Preview"},
             {DebugPanel::kSurface, true, "Surface"}, {DebugPanel::kBrush, true, "Brush"},
             {DebugPanel::kTelemetry, true, "Telemetry"}, {DebugPanel::kInspection, false, "Inspection (Unavailable)"}}};
}
std::optional<CommandReceipt> DebugController::submit(DebugCommand command) {
    return context_.commands == nullptr ? std::nullopt : context_.commands->admit(std::move(command));
}

bool buildImGuiPanels(const DebugSnapshot& snapshot, int selectedTool,
                      canvas::runtime::RuntimeFacade* runtime,
                      canvas::runtime::AxiomDebugControl* axiomDebug,
                      canvas::runtime::PlatformDebugControl* platform) {
    bool submittedControl = false;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(410.0f, 560.0f), ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin("Axiom Debug UI", nullptr, flags)) {
        ImGui::End();
        return false;
    }
    ImGui::TextColored(ImVec4(0.96f, 0.73f, 0.27f, 1.0f),
                       "Axiom Debug UI / common ImGui + Skia");
    ImGui::Text("P32 controller / reference profile");
    ImGui::Separator();
    const std::array<std::pair<const char*, Capability>, 8> panels{{
        {"Overview", Capability::kTelemetry},
        {"Input", Capability::kInput},
        {"Canvas", Capability::kCanonicalSurface},
        {"Arc Preview", Capability::kArcPreviewSurface},
        {"Surface", Capability::kSurfaceMode},
        {"Brush", Capability::kCanonicalSurface},
        {"Telemetry", Capability::kTelemetry},
        {"Inspection", Capability::kInspection},
    }};
    for (const auto& panel : panels) {
        const auto state = snapshot.capability(panel.second);
        const bool available = state == CapabilityState::kAvailable;
        const char* suffix = available ? "Available" :
            (state == CapabilityState::kDegraded ? "Degraded" : "Unavailable");
        ImGui::TextColored(available ? ImVec4(0.84f, 0.9f, 0.95f, 1.0f)
                                    : ImVec4(0.55f, 0.58f, 0.62f, 1.0f),
                           "%s  %s", panel.first, suffix);
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.96f, 0.73f, 0.27f, 1.0f), "Brush / Eraser");
    const std::array<std::pair<const char*, int>, 6> tools{{
        {"Vector", 4101}, {"Marker", 4102}, {"Chalk", 4103},
        {"Membrane", 4104}, {"Object Eraser", 4105}, {"Partial Eraser", 4106},
    }};
    static std::uint64_t nextRequestId = 1;
    for (const auto& tool : tools) {
        const bool selected = selectedTool == tool.second;
        if (ImGui::Selectable(tool.first, selected) && runtime != nullptr) {
            canvas::runtime::ProductControlRequest request{};
            request.action = tool.second >= 4105
                ? canvas::runtime::ProductControlAction::kSetEraser
                : canvas::runtime::ProductControlAction::kSetBrush;
            request.requestId = nextRequestId++;
            request.runtimeGeneration = snapshot.stamp.runtimeGeneration;
            request.deadlineSequence = snapshot.stamp.sequence + 120U;
            request.toolId = static_cast<std::uint32_t>(tool.second);
            request.brushId = static_cast<std::uint32_t>(tool.second - 4100);
            request.eraserId = static_cast<std::uint32_t>(tool.second - 4104);
            (void)runtime->submitProductControl(request);
            submittedControl = true;
        }
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.96f, 0.73f, 0.27f, 1.0f), "Canonical surface mode");
    const auto requestSurface = [&](canvas::runtime::SurfaceMode mode) {
        if (platform == nullptr) return;
        canvas::runtime::SurfaceModeRequest request{};
        request.requestId = nextRequestId++;
        request.target = canvas::runtime::SurfaceRole::kCanonicalCanvas;
        request.mode = mode;
        request.expectedGeneration = snapshot.stamp.surfaceGeneration;
        request.deadlineSequence = snapshot.stamp.sequence + 120U;
        (void)platform->enqueueSurfaceMode(request);
        submittedControl = true;
    };
    if (ImGui::Button("Platform default")) requestSurface(canvas::runtime::SurfaceMode::kPlatformDefault);
    ImGui::SameLine();
    if (ImGui::Button("CPU reference")) requestSurface(canvas::runtime::SurfaceMode::kCpuReference);
    ImGui::SameLine();
    if (ImGui::Button("GPU default")) requestSurface(canvas::runtime::SurfaceMode::kGpuDefault);
    const char* mode = snapshot.canonicalSurfaceMode == canvas::runtime::SurfaceMode::kCpuReference
        ? "CPU reference" : (snapshot.canonicalSurfaceMode == canvas::runtime::SurfaceMode::kGpuDefault
        ? "GPU default" : "Platform default");
    ImGui::Text("resolved: %s / canonical generation %llu", mode,
                static_cast<unsigned long long>(snapshot.stamp.surfaceGeneration));
    if (snapshot.surfaceControlRequestId != 0U) {
        const char* receipt = "failed";
        switch (snapshot.surfaceControlState) {
        case canvas::runtime::SurfaceControlState::kQueued: receipt = "queued"; break;
        case canvas::runtime::SurfaceControlState::kApplied: receipt = "applied"; break;
        case canvas::runtime::SurfaceControlState::kUnsupported: receipt = "unsupported"; break;
        case canvas::runtime::SurfaceControlState::kStaleGeneration: receipt = "stale-generation"; break;
        case canvas::runtime::SurfaceControlState::kQueueFull: receipt = "queue-full"; break;
        case canvas::runtime::SurfaceControlState::kExpired: receipt = "expired"; break;
        case canvas::runtime::SurfaceControlState::kUnavailable: receipt = "unavailable"; break;
        case canvas::runtime::SurfaceControlState::kFailed: receipt = "failed"; break;
        }
        ImGui::Text("surface request %llu: %s / generation %llu",
                    static_cast<unsigned long long>(snapshot.surfaceControlRequestId), receipt,
                    static_cast<unsigned long long>(snapshot.surfaceControlGeneration));
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.96f, 0.73f, 0.27f, 1.0f), "Runtime debug controls");
    const auto enqueueDebug = [&](canvas::runtime::AxiomDebugCommandKind kind) {
        if (axiomDebug == nullptr) return;
        canvas::runtime::AxiomDebugCommand command{};
        command.requestId = nextRequestId++;
        command.kind = kind;
        command.expectedRuntimeGeneration = snapshot.stamp.runtimeGeneration;
        command.expectedDocumentGeneration = snapshot.stamp.documentGeneration;
        command.deadlineSequence = snapshot.stamp.sequence + 120U;
        (void)axiomDebug->enqueue(command);
        submittedControl = true;
    };
    if (ImGui::Button("Force full redraw")) {
        enqueueDebug(canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset rolling metrics")) {
        enqueueDebug(canvas::runtime::AxiomDebugCommandKind::kResetRollingMetrics);
    }
    ImGui::Separator();
    ImGui::Text("gen %llu  seq %llu  pointers %u",
                static_cast<unsigned long long>(snapshot.stamp.generation),
                static_cast<unsigned long long>(snapshot.stamp.sequence),
                snapshot.activePointerCount);
    ImGui::Text("canonical %llu  preview %llu",
                static_cast<unsigned long long>(snapshot.canonicalRevision),
                static_cast<unsigned long long>(snapshot.previewRevision));
    ImGui::Text("input batches %llu  handoffs %llu",
                static_cast<unsigned long long>(snapshot.inputBatchCount),
                static_cast<unsigned long long>(snapshot.handoffCount));
    ImGui::Text("present %llu  lost %llu  frame %.2f ms",
                static_cast<unsigned long long>(snapshot.presentCount),
                static_cast<unsigned long long>(snapshot.surfaceLostCount),
                snapshot.frameMs);
    ImGui::Text("surface: %s  sample %.1f Hz  queue %.2f ms",
                snapshot.surfaceAvailable ? "available" : "unavailable",
                snapshot.sampleHz, snapshot.queueAgeMs);
    ImGui::Text("Arc presenter: %s", snapshot.arcPresenterActive ? "active" : "idle");
    ImGui::End();
    return submittedControl;
}

#if defined(CANVAS_DEBUG_UI_HAS_SKIA)
namespace {
SkColor toSkColor(ImU32 color) noexcept {
    return SkColorSetARGB(static_cast<U8CPU>((color >> IM_COL32_A_SHIFT) & 0xffU),
                          static_cast<U8CPU>((color >> IM_COL32_R_SHIFT) & 0xffU),
                          static_cast<U8CPU>((color >> IM_COL32_G_SHIFT) & 0xffU),
                          static_cast<U8CPU>((color >> IM_COL32_B_SHIFT) & 0xffU));
}
}

bool ImGuiSkiaRenderer::render(ImDrawData* drawData, SkSurface* surface,
                               SkImage* fontTexture) noexcept {
    if (drawData == nullptr || surface == nullptr || fontTexture == nullptr) return false;
    SkCanvas* canvas = surface->getCanvas();
    if (canvas == nullptr) return false;
    canvas->clear(SK_ColorTRANSPARENT);
    const float sx = drawData->FramebufferScale.x > 0.0f ? drawData->FramebufferScale.x : 1.0f;
    const float sy = drawData->FramebufferScale.y > 0.0f ? drawData->FramebufferScale.y : 1.0f;
    canvas->save();
    canvas->scale(sx, sy);
    canvas->translate(-drawData->DisplayPos.x, -drawData->DisplayPos.y);
    for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex) {
        const ImDrawList* list = drawData->CmdLists[listIndex];
        for (const ImDrawCmd& command : list->CmdBuffer) {
            if (command.UserCallback != nullptr) continue;
            const ImVec4 clip = command.ClipRect;
            const float clipWidth = clip.z - clip.x;
            const float clipHeight = clip.w - clip.y;
            if (clipWidth <= 0.0f || clipHeight <= 0.0f || command.ElemCount == 0) continue;
            canvas->save();
            canvas->clipRect(SkRect::MakeLTRB(clip.x, clip.y, clip.z, clip.w));
            const unsigned int first = command.IdxOffset;
            const unsigned int end = first + command.ElemCount;
            constexpr unsigned int kMaxTrianglesPerBatch = 20000;
            for (unsigned int cursor = first; cursor < end;) {
                const unsigned int remaining = end - cursor;
                const unsigned int triangles = (std::min)(remaining / 3U, kMaxTrianglesPerBatch);
                if (triangles == 0U) break;
                const unsigned int vertexCount = triangles * 3U;
                std::vector<SkPoint> positions(vertexCount);
                std::vector<SkPoint> texCoords(vertexCount);
                std::vector<SkColor> colors(vertexCount);
                std::vector<std::uint16_t> indices(vertexCount);
                for (unsigned int i = 0; i < vertexCount; ++i) {
                    const ImDrawIdx index = list->IdxBuffer[cursor + i];
                    const ImDrawVert& vertex = list->VtxBuffer[command.VtxOffset + index];
                    positions[i] = SkPoint::Make(vertex.pos.x, vertex.pos.y);
                    // Skia Viewer keeps ImGui UVs normalized and maps them into
                    // the A8 atlas through the shader's local matrix.
                    texCoords[i] = SkPoint::Make(vertex.uv.x, vertex.uv.y);
                    colors[i] = toSkColor(vertex.col);
                    indices[i] = static_cast<std::uint16_t>(i);
                }
                SkPaint paint;
                paint.setAntiAlias(false);
                paint.setColor(SK_ColorWHITE);
                const SkMatrix atlasMatrix = SkMatrix::Scale(
                    1.0f / static_cast<float>(fontTexture->width()),
                    1.0f / static_cast<float>(fontTexture->height()));
                paint.setShader(fontTexture->makeShader(
                    SkTileMode::kClamp, SkTileMode::kClamp,
                    SkSamplingOptions(SkFilterMode::kLinear), atlasMatrix));
                auto vertices = SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode,
                                                      static_cast<int>(vertexCount),
                                                      positions.data(), texCoords.data(),
                                                      colors.data(), static_cast<int>(vertexCount),
                                                      indices.data());
                if (vertices) canvas->drawVertices(vertices, SkBlendMode::kModulate, paint);
                cursor += triangles * 3U;
            }
            canvas->restore();
        }
    }
    canvas->restore();
    return true;
}
#endif
}  // namespace canvas::debug_ui
