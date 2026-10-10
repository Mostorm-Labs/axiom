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
    static_cast<void>(axiomDebug);
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
    const std::array<std::pair<const char*, int>, 7> tools{{
        {"Vector", 4101}, {"Marker", 4102}, {"Chalk", 4103},
        {"Membrane", 4104}, {"Object Eraser", 4105}, {"Partial Eraser", 4106},
        {"Pan", 4107},
    }};
    static std::uint64_t nextRequestId = 1;
    const auto availabilityLabel = [](DebugAvailability state) {
        return state == DebugAvailability::kAvailable ? "Available" :
            state == DebugAvailability::kDegraded ? "Degraded" :
            state == DebugAvailability::kUnsupported ? "Unsupported" : "Unavailable";
    };
    if (ImGui::BeginTabBar("##debug_tabs")) {
      if (ImGui::BeginTabItem("Overview")) {
        ImGui::Text("Input: %s  Canvas: %s  Surface: %s",
                    availabilityLabel(snapshot.arc.availability),
                    availabilityLabel(snapshot.platform.availability),
                    availabilityLabel(snapshot.platform.availability));
        ImGui::Text("Arc Preview: %s  Telemetry: %s  Inspection: %s",
                    availabilityLabel(snapshot.arc.availability),
                    availabilityLabel(snapshot.telemetry.availability),
                    "Unsupported");
        ImGui::Text("runtime gen %llu / document gen %llu / view gen %llu",
                    static_cast<unsigned long long>(snapshot.stamp.runtimeGeneration),
                    static_cast<unsigned long long>(snapshot.stamp.documentGeneration),
                    static_cast<unsigned long long>(snapshot.stamp.viewGeneration));
        ImGui::Text("coherence: %s / snapshot #%llu",
                    snapshot.coherence == SnapshotCoherence::kCoherent ? "Coherent" :
                    snapshot.coherence == SnapshotCoherence::kMixedGeneration ? "MixedGeneration" : "Stale",
                    static_cast<unsigned long long>(snapshot.stamp.snapshotSequence));
        ImGui::Text("canonical %llu / preview %llu / presents %llu",
                    static_cast<unsigned long long>(snapshot.axiom.value.document.canonicalOperationCount),
                    static_cast<unsigned long long>(snapshot.arc.value.previewRevision),
                    static_cast<unsigned long long>(snapshot.platform.value.presentCount));
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Canvas / Selection")) {
        bool selectionMode = snapshot.product.value.selection.enabled;
        if (ImGui::Checkbox("Selection mode", &selectionMode) && runtime != nullptr) {
            (void)runtime->setSelectionMode(selectionMode, nextRequestId++,
                                            snapshot.product.value.identity.runtimeGeneration);
            submittedControl = true;
        }
        ImGui::Text("selected objects: %u", snapshot.product.value.selection.selectedObjectCount);
        ImGui::Text("primary object: %llu",
                    static_cast<unsigned long long>(snapshot.product.value.selection.primaryObject));
        ImGui::Text("Click the canvas to select the frontmost eligible object.");
        ImGui::Text("EditingOverlay is per-view and transient.");
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("RuntimeFacade")) {
        ImGui::Text("Product controls are submitted through RuntimeFacade.");
        ImGui::BeginDisabled(runtime == nullptr || !snapshot.product.value.history.canUndo);
        if (ImGui::Button("Undo (Ctrl+Z)")) {
            (void)runtime->undo(nextRequestId++, snapshot.product.value.identity.runtimeGeneration);
            submittedControl = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(runtime == nullptr || !snapshot.product.value.history.canRedo);
        if (ImGui::Button("Redo (Ctrl+Y)")) {
            (void)runtime->redo(nextRequestId++, snapshot.product.value.identity.runtimeGeneration);
            submittedControl = true;
        }
        ImGui::EndDisabled();
        if (snapshot.activity.productControl.has_value()) {
            const auto& receipt = *snapshot.activity.productControl;
            const char* state = "rejected";
            switch (receipt.state) {
            case canvas::runtime::ProductControlState::kApplied: state = "applied"; break;
            case canvas::runtime::ProductControlState::kQueued: state = "queued"; break;
            case canvas::runtime::ProductControlState::kUnsupported: state = "unsupported"; break;
            case canvas::runtime::ProductControlState::kFailed: state = "failed"; break;
            case canvas::runtime::ProductControlState::kRejected: state = "rejected"; break;
            }
            ImGui::Text("product request %llu: %s",
                        static_cast<unsigned long long>(receipt.requestId), state);
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Brush / Eraser")) {
        for (const auto& tool : tools) {
            const bool selected = selectedTool == tool.second;
            if (ImGui::Selectable(tool.first, selected) && runtime != nullptr) {
                canvas::runtime::CanvasControlRequest request{};
                request.clientId = 1U;
                request.requestId = nextRequestId++;
                request.deadlineSequence = snapshot.stamp.sequence + 120U;
                const auto targets = runtime->mountedCanvasTargets();
                request.target = targets.empty()
                    ? canvas::runtime::CanvasTargetKey{snapshot.stamp.runtimeGeneration, 1U,
                                                       snapshot.stamp.viewGeneration, snapshot.stamp.surfaceGeneration}
                    : targets.front();
                request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectTool;
                request.payload.tool = tool.second == 4107
                    ? canvas::runtime::CanvasToolKind::kPan
                    : tool.second == 4105 || tool.second == 4106
                        ? canvas::runtime::CanvasToolKind::kEraser
                        : canvas::runtime::CanvasToolKind::kInk;
                if (tool.second >= 4105 && tool.second <= 4106) {
                    request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kEraserOptions;
                    request.payload.eraser.mode = canvas::runtime::OptionPatch<std::uint32_t>::set(
                        tool.second == 4105 ? 1U : 2U);
                    request.payload.eraser.diameterLogicalPx = canvas::runtime::OptionPatch<float>::keep();
                } else if (tool.second != 4107 && tool.second >= 4101 && tool.second <= 4104) {
                    request.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectPreset;
                    request.payload.preset = {tool.second == 4101 ? "vector-solid-v1" :
                                              tool.second == 4102 ? "marker-flat-v1" :
                                              tool.second == 4103 ? "chalk-grain-v1" : "membrane-v1",
                                              tool.second == 4103 ? 4U : 1U};
                }
                (void)runtime->submitCanvasControl(request);
                submittedControl = true;
            }
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Input")) {
        ImGui::Text("active pointers: %llu", static_cast<unsigned long long>(snapshot.arc.value.activePointerCount));
        ImGui::Text("input batches: %llu / sample %.1f Hz",
                    static_cast<unsigned long long>(snapshot.arc.value.inputBatchCount), snapshot.telemetry.value.sampleHz);
        ImGui::Text("queue age %.2f ms / frame %.2f ms",
                    snapshot.telemetry.value.queueAgeMs, snapshot.telemetry.value.frameMs);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Surface")) {
        ImGui::TextColored(ImVec4(0.96f, 0.73f, 0.27f, 1.0f), "Canonical surface mode");
    const auto requestSurface = [&](canvas::runtime::SurfaceMode mode) {
        if (platform == nullptr) return;
        canvas::runtime::SurfaceModeRequest request{};
        request.requestId = nextRequestId++;
        request.target = canvas::runtime::SurfaceRole::kCanonicalCanvas;
        request.mode = mode;
        request.expectedGeneration = snapshot.product.value.identity.surfaceGeneration;
        request.deadlineSequence = snapshot.stamp.sequence + 120U;
        (void)platform->enqueueSurfaceMode(request);
        submittedControl = true;
    };
    if (ImGui::Button("Platform default")) requestSurface(canvas::runtime::SurfaceMode::kPlatformDefault);
    ImGui::SameLine();
    if (ImGui::Button("CPU reference")) requestSurface(canvas::runtime::SurfaceMode::kCpuReference);
    ImGui::SameLine();
    if (ImGui::Button("GPU default")) requestSurface(canvas::runtime::SurfaceMode::kGpuDefault);
    const char* mode = snapshot.platform.value.canonicalSurfaceMode == canvas::runtime::SurfaceMode::kCpuReference
        ? "CPU reference" : (snapshot.platform.value.canonicalSurfaceMode == canvas::runtime::SurfaceMode::kGpuDefault
        ? "GPU default" : "Platform default");
    ImGui::Text("resolved: %s / canonical generation %llu", mode,
                static_cast<unsigned long long>(snapshot.platform.value.canonicalSurfaceGeneration));
    if (snapshot.activity.surfaceControl.has_value()) {
        const auto& surfaceReceipt = *snapshot.activity.surfaceControl;
        const char* receipt = "failed";
        switch (surfaceReceipt.state) {
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
                    static_cast<unsigned long long>(surfaceReceipt.requestId), receipt,
                    static_cast<unsigned long long>(surfaceReceipt.generation));
      }
      ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Diagnostics")) {
        ImGui::Text("canonical %llu / preview %llu",
                    static_cast<unsigned long long>(snapshot.axiom.value.document.canonicalOperationCount),
                    static_cast<unsigned long long>(snapshot.arc.value.previewRevision));
        ImGui::Text("handoffs %llu / presents %llu / lost %llu",
                    static_cast<unsigned long long>(snapshot.arc.value.handoffCount),
                    static_cast<unsigned long long>(snapshot.platform.value.presentCount),
                    static_cast<unsigned long long>(snapshot.platform.value.lostCount));
        ImGui::Text("surface: %s / Arc presenter: %s",
                    snapshot.platform.value.surfaceAvailable ? "available" : "unavailable",
                    snapshot.arc.value.previewActive ? "active" : "idle");
        ImGui::Text("overlay selection: %u object(s)", snapshot.product.value.selection.selectedObjectCount);
      ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
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
                // ImGui stores normalized UVs; the local matrix maps them into
                // the atlas image's normalized shader space, as in Skia's
                // Viewer ImGuiLayer.
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
