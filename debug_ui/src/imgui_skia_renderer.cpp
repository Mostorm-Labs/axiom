#include "canvas/debug_ui/imgui_skia_renderer.hpp"
#include "canvas/debug_ui/snapshot.hpp"
#if defined(CANVAS_DEBUG_UI_HAS_SKIA)
#include "imgui.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImage.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/core/SkVertices.h"
#include "include/core/SkSamplingOptions.h"
#include <algorithm>
#include <vector>
#endif

namespace canvas::debug_ui {
std::uint64_t ImGuiSkiaRenderer::render(const DebugSnapshot& snapshot) noexcept { return snapshot.stamp.sequence; }
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
