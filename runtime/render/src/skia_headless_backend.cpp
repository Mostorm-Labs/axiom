#include "canvas/render/skia_headless_backend.hpp"

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>

namespace canvas::render {
namespace {

bool finite(double value) noexcept { return std::isfinite(value); }
bool integral(double value) noexcept {
    return finite(value) && std::floor(value) == value;
}

bool scalarRange(double value) noexcept {
    return finite(value) && value >= -std::numeric_limits<float>::max() &&
           value <= std::numeric_limits<float>::max();
}

bool pointInScalarRange(const semantic::Vec2& point) noexcept {
    return scalarRange(point.x) && scalarRange(point.y);
}

bool validAffine(const WorldToViewAffine& affine) noexcept {
    return scalarRange(affine.a) && scalarRange(affine.b) &&
           scalarRange(affine.c) && scalarRange(affine.d) &&
           scalarRange(affine.tx) && scalarRange(affine.ty) &&
           integral(affine.a) && integral(affine.b) && integral(affine.c) &&
           integral(affine.d) && integral(affine.tx) && integral(affine.ty);
}

bool validRect(const foundation::WorldRect& rect) noexcept {
    return rect.isFiniteAndOrdered() && integral(rect.left) && integral(rect.top) &&
           integral(rect.right) && integral(rect.bottom);
}

bool validUtf8(const std::string& text) noexcept {
    for (std::size_t index = 0; index < text.size();) {
        const auto lead = static_cast<std::uint8_t>(text[index]);
        std::size_t width = 0U;
        if (lead <= 0x7fU) width = 1U;
        else if (lead >= 0xc2U && lead <= 0xdfU) width = 2U;
        else if (lead >= 0xe0U && lead <= 0xefU) width = 3U;
        else if (lead >= 0xf0U && lead <= 0xf4U) width = 4U;
        else return false;
        if (index + width > text.size()) return false;
        for (std::size_t offset = 1U; offset < width; ++offset) {
            if ((static_cast<std::uint8_t>(text[index + offset]) & 0xc0U) != 0x80U) return false;
        }
        if (width >= 3U) {
            const auto second = static_cast<std::uint8_t>(text[index + 1U]);
            if ((lead == 0xe0U && second < 0xa0U) ||
                (lead == 0xedU && second >= 0xa0U) ||
                (lead == 0xf0U && second < 0x90U) ||
                (lead == 0xf4U && second > 0x8fU)) return false;
        }
        index += width;
    }
    return true;
}

SkMatrix matrix(const WorldToViewAffine& affine) {
    return SkMatrix::MakeAll(static_cast<float>(affine.a), static_cast<float>(affine.c),
                             static_cast<float>(affine.tx), static_cast<float>(affine.b),
                             static_cast<float>(affine.d), static_cast<float>(affine.ty),
                             0.0F, 0.0F, 1.0F);
}

SkMatrix matrix(const semantic::Transform2D& transform) {
    return SkMatrix::MakeAll(static_cast<float>(transform.a), static_cast<float>(transform.c),
                             static_cast<float>(transform.tx), static_cast<float>(transform.b),
                             static_cast<float>(transform.d), static_cast<float>(transform.ty),
                             0.0F, 0.0F, 1.0F);
}

SkRect rect(const foundation::WorldRect& value) {
    return SkRect::MakeLTRB(value.left, value.top, value.right, value.bottom);
}

SkColor4f color(const semantic::ColorValue& value) {
    return SkColor4f{value.r, value.g, value.b, value.a};
}

semantic::ColorValue propertyColor(const RuntimeSceneRecord& record,
                                   std::uint32_t fieldId,
                                   semantic::ColorValue fallback) {
    for (const auto& property : record.properties.entries) {
        if (property.field_id != fieldId) continue;
        if (fieldId == 0x00000100U) {
            if (const auto* fill = std::get_if<semantic::FillStyleValue>(&property.value)) {
                if (const auto* solid = std::get_if<semantic::SolidFill>(fill)) return solid->color;
            }
        }
        if (fieldId == 0x00000101U) {
            if (const auto* stroke = std::get_if<semantic::StrokeStyleValue>(&property.value)) {
                if (const auto* solid = std::get_if<semantic::SolidStroke>(stroke)) return solid->color;
            }
        }
    }
    return fallback;
}

void drawPath(SkCanvas& canvas, const semantic::VectorPathGeometry& geometry,
              const SkPaint& paint) {
    SkPathBuilder path(geometry.fill_rule == semantic::FillRule::kEvenOdd
                           ? SkPathFillType::kEvenOdd
                           : SkPathFillType::kWinding);
    for (const auto& command : geometry.commands) {
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, semantic::MoveTo>) path.moveTo(value.point.x, value.point.y);
            else if constexpr (std::is_same_v<T, semantic::LineTo>) path.lineTo(value.end.x, value.end.y);
            else if constexpr (std::is_same_v<T, semantic::QuadTo>) path.quadTo(value.control.x, value.control.y, value.end.x, value.end.y);
            else if constexpr (std::is_same_v<T, semantic::CubicTo>) path.cubicTo(value.control1.x, value.control1.y, value.control2.x, value.control2.y, value.end.x, value.end.y);
            else if constexpr (std::is_same_v<T, semantic::ClosePath>) path.close();
        }, command);
    }
    canvas.drawPath(path.detach(), paint);
}

void drawSolidRect(SkCanvas& canvas, const foundation::WorldRect& bounds,
                   const semantic::ColorValue& value) {
    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setColor4f(color(value));
    paint.setStyle(SkPaint::kFill_Style);
    canvas.drawRect(rect(bounds), paint);
}

bool validatePath(const semantic::VectorPathGeometry& geometry) noexcept {
    for (const auto& command : geometry.commands) {
        bool ok = true;
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            auto pointOk = [](const semantic::Vec2& point) {
                return pointInScalarRange(point) && integral(point.x) && integral(point.y);
            };
            if constexpr (std::is_same_v<T, semantic::MoveTo>) ok = pointOk(value.point);
            else if constexpr (std::is_same_v<T, semantic::LineTo>) ok = pointOk(value.end);
            else if constexpr (std::is_same_v<T, semantic::QuadTo>) ok = pointOk(value.control) && pointOk(value.end);
            else if constexpr (std::is_same_v<T, semantic::CubicTo>) ok = pointOk(value.control1) && pointOk(value.control2) && pointOk(value.end);
        }, command);
        if (!ok) return false;
    }
    return true;
}

bool pathInScalarRange(const semantic::VectorPathGeometry& geometry) noexcept {
    for (const auto& command : geometry.commands) {
        bool ok = true;
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, semantic::MoveTo>) ok = pointInScalarRange(value.point);
            else if constexpr (std::is_same_v<T, semantic::LineTo>) ok = pointInScalarRange(value.end);
            else if constexpr (std::is_same_v<T, semantic::QuadTo>) ok = pointInScalarRange(value.control) && pointInScalarRange(value.end);
            else if constexpr (std::is_same_v<T, semantic::CubicTo>) ok = pointInScalarRange(value.control1) && pointInScalarRange(value.control2) && pointInScalarRange(value.end);
        }, command);
        if (!ok) return false;
    }
    return true;
}

HeadlessSubmissionIssue validateCommand(const ReferenceTraversalEntry& entry) noexcept {
    return std::visit([&](const auto& command) -> HeadlessSubmissionIssue {
        using T = std::decay_t<decltype(command)>;
        if constexpr (std::is_same_v<T, VectorPathReferenceCommand>) {
            if (!pathInScalarRange(command.content.geometry)) return HeadlessSubmissionIssue::kOverflow;
            if (!validatePath(command.content.geometry)) return HeadlessSubmissionIssue::kUnsupportedGeometry;
        } else if constexpr (std::is_same_v<T, RichTextReferenceCommand>) {
            for (const auto& paragraph : command.content.document.paragraphs) {
                for (const auto& run : paragraph.runs) {
                    if (!validUtf8(run.text) || !integral(run.style.font_size)) {
                        return HeadlessSubmissionIssue::kUnsupportedGeometry;
                    }
                }
            }
        } else if constexpr (std::is_same_v<T, VectorStrokeReferenceCommand>) {
            const auto* data = std::get_if<semantic::VectorStrokeData>(&command.content.stroke.data);
            if (data == nullptr || !integral(command.content.stroke.brush.nominal_size) ||
                !finite(command.content.stroke.brush.opacity)) {
                return HeadlessSubmissionIssue::kUnsupportedGeometry;
            }
            for (const auto& sample : data->samples) {
                if (!pointInScalarRange(sample.position)) return HeadlessSubmissionIssue::kOverflow;
                if (!integral(sample.position.x) || !integral(sample.position.y)) {
                    return HeadlessSubmissionIssue::kUnsupportedGeometry;
                }
            }
        } else if constexpr (std::is_same_v<T, DabStrokeReferenceCommand>) {
            const auto* data = std::get_if<semantic::DabStrokeData>(&command.content.stroke.data);
            if (data == nullptr || !finite(command.content.stroke.brush.opacity)) {
                return HeadlessSubmissionIssue::kUnsupportedGeometry;
            }
            for (const auto& dab : data->dabs) {
                const double half = dab.size / 2.0;
                if (!pointInScalarRange(dab.center) || !scalarRange(dab.size)) {
                    return HeadlessSubmissionIssue::kOverflow;
                }
                if (!integral(dab.center.x - half) || !integral(dab.center.y - half) ||
                    !integral(dab.center.x + half) || !integral(dab.center.y + half) ||
                    !finite(dab.opacity)) {
                    return HeadlessSubmissionIssue::kUnsupportedGeometry;
                }
            }
        } else if constexpr (std::is_same_v<T, ConnectorReferenceCommand>) {
            const auto* start = std::get_if<semantic::FreePointEndpoint>(&command.content.start.value);
            const auto* end = std::get_if<semantic::FreePointEndpoint>(&command.content.end.value);
            if (start == nullptr || end == nullptr) return HeadlessSubmissionIssue::kUnsupportedGeometry;
            if (!pointInScalarRange(start->point) || !pointInScalarRange(end->point)) {
                return HeadlessSubmissionIssue::kOverflow;
            }
            if (!integral(start->point.x) || !integral(start->point.y) ||
                !integral(end->point.x) || !integral(end->point.y)) {
                return HeadlessSubmissionIssue::kUnsupportedGeometry;
            }
        } else if constexpr (std::is_same_v<T, StickyReferenceCommand>) {
            if (!scalarRange(command.content.width) || !scalarRange(command.content.height)) {
                return HeadlessSubmissionIssue::kOverflow;
            }
            if (!integral(command.content.width) || !integral(command.content.height)) {
                return HeadlessSubmissionIssue::kUnsupportedGeometry;
            }
        }
        return HeadlessSubmissionIssue::kNone;
    }, entry.command);
}

void drawCommand(SkCanvas& canvas, const ReferenceTraversalEntry& entry) {
    const auto& record = entry.record;
    std::visit([&](const auto& command) {
        using T = std::decay_t<decltype(command)>;
        if constexpr (std::is_same_v<T, ShapeReferenceCommand>) {
            drawSolidRect(canvas, record.visualBounds,
                          propertyColor(record, 0x00000100U, {1.0F, 0.0F, 0.0F, 1.0F}));
        } else if constexpr (std::is_same_v<T, ImageReferenceCommand>) {
            std::uint64_t id = 0U;
            for (std::size_t index = 0; index < sizeof(id); ++index) {
                id |= static_cast<std::uint64_t>(command.content.resource_id.value.bytes[index])
                      << (index * 8U);
            }
            const semantic::ColorValue bright{0.0F, 1.0F, 0.0F, 1.0F};
            const semantic::ColorValue dark{0.0F, 128.0F / 255.0F, 0.0F, 1.0F};
            const auto width = static_cast<std::uint32_t>(record.visualBounds.right - record.visualBounds.left);
            const auto height = static_cast<std::uint32_t>(record.visualBounds.bottom - record.visualBounds.top);
            for (std::uint32_t y = 0; y < height; y += 4U) {
                for (std::uint32_t x = 0; x < width; x += 4U) {
                    const bool isBright = (((x / 4U) + (y / 4U) + id) % 2U) == 0U;
                    drawSolidRect(canvas, foundation::WorldRect{record.visualBounds.left + static_cast<float>(x), record.visualBounds.top + static_cast<float>(y), record.visualBounds.left + static_cast<float>(std::min(x + 4U, width)), record.visualBounds.top + static_cast<float>(std::min(y + 4U, height))}, isBright ? bright : dark);
                }
            }
        } else if constexpr (std::is_same_v<T, VectorPathReferenceCommand>) {
            SkPaint paint; paint.setAntiAlias(false); paint.setColor4f(color(propertyColor(record, 0x00000100U, {0.0F, 0.0F, 1.0F, 1.0F}))); drawPath(canvas, command.content.geometry, paint);
        } else if constexpr (std::is_same_v<T, RichTextReferenceCommand>) {
            std::uint32_t x = static_cast<std::uint32_t>(record.visualBounds.left);
            for (const auto& paragraph : command.content.document.paragraphs) {
                for (const auto& run : paragraph.runs) {
                    const auto style = run.style.color;
                    for (std::size_t index = 0; index < run.text.size();) {
                        const auto lead = static_cast<std::uint8_t>(run.text[index]);
                        const std::size_t scalarWidth = lead <= 0x7fU ? 1U :
                            (lead <= 0xdfU ? 2U : (lead <= 0xefU ? 3U : 4U));
                        drawSolidRect(canvas, foundation::WorldRect{static_cast<float>(x), record.visualBounds.top, static_cast<float>(x + 8U), record.visualBounds.top + 12.0F}, style);
                        x += 10U;
                        index += scalarWidth;
                    }
                }
            }
        } else if constexpr (std::is_same_v<T, VectorStrokeReferenceCommand>) {
            const auto& stroke = command.content.stroke;
            const auto* data = std::get_if<semantic::VectorStrokeData>(&stroke.data);
            if (data == nullptr) return;
            auto strokeColor = stroke.brush.color;
            strokeColor.a *= stroke.brush.opacity;
            SkPaint paint; paint.setAntiAlias(false); paint.setColor4f(color(strokeColor)); paint.setStyle(SkPaint::kStroke_Style); paint.setStrokeWidth(static_cast<float>(stroke.brush.nominal_size)); paint.setStrokeCap(SkPaint::kButt_Cap);
            for (std::size_t i = 1; i < data->samples.size(); ++i) canvas.drawLine(data->samples[i - 1].position.x, data->samples[i - 1].position.y, data->samples[i].position.x, data->samples[i].position.y, paint);
        } else if constexpr (std::is_same_v<T, DabStrokeReferenceCommand>) {
            const auto& stroke = command.content.stroke;
            const auto* data = std::get_if<semantic::DabStrokeData>(&stroke.data);
            if (data == nullptr) return;
            for (const auto& dab : data->dabs) {
                auto dabColor = stroke.brush.color;
                dabColor.a *= stroke.brush.opacity * dab.opacity;
                drawSolidRect(canvas, foundation::WorldRect{static_cast<float>(dab.center.x - dab.size / 2.0), static_cast<float>(dab.center.y - dab.size / 2.0), static_cast<float>(dab.center.x + dab.size / 2.0), static_cast<float>(dab.center.y + dab.size / 2.0)}, dabColor);
            }
        } else if constexpr (std::is_same_v<T, ConnectorReferenceCommand>) {
            const auto* start = std::get_if<semantic::FreePointEndpoint>(&command.content.start.value);
            const auto* end = std::get_if<semantic::FreePointEndpoint>(&command.content.end.value);
            if (start == nullptr || end == nullptr) return;
            SkPaint paint; paint.setAntiAlias(false); paint.setColor4f(color(propertyColor(record, 0x00000101U, {1.0F, 128.0F / 255.0F, 0.0F, 1.0F}))); paint.setStyle(SkPaint::kStroke_Style); paint.setStrokeWidth(4.0F); paint.setStrokeCap(SkPaint::kButt_Cap);
            if (command.content.routing == semantic::ConnectorRouting::kOrthogonal) {
                canvas.drawLine(start->point.x, start->point.y, end->point.x,
                                start->point.y, paint);
                canvas.drawLine(end->point.x, start->point.y, end->point.x,
                                end->point.y, paint);
            } else {
                canvas.drawLine(start->point.x, start->point.y, end->point.x,
                                end->point.y, paint);
            }
        } else if constexpr (std::is_same_v<T, StickyReferenceCommand>) {
            drawSolidRect(canvas, foundation::WorldRect{record.visualBounds.left, record.visualBounds.top, record.visualBounds.left + static_cast<float>(command.content.width), record.visualBounds.top + static_cast<float>(command.content.height)}, {1.0F, 1.0F, 0.5F, 1.0F});
        } else if constexpr (std::is_same_v<T, GroupReferenceCommand>) {
            // Group is an ordered traversal entry with no pixel contribution.
        }
    }, entry.command);
}

void eraseMasks(SkCanvas& canvas, const ReferenceTraversalEntry& entry) {
    for (const auto& mask : entry.record.eraseMasks) {
        const auto* filled = std::get_if<semantic::FilledPathMask>(&mask.geometry);
        if (filled == nullptr) return;
        SkPaint paint; paint.setAntiAlias(false); paint.setBlendMode(SkBlendMode::kClear); drawPath(canvas, filled->path, paint);
    }
}

std::string digest(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t value = 0xcbf29ce484222325ULL;
    for (std::uint8_t byte : bytes) { value ^= byte; value *= 0x100000001b3ULL; }
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "fnv1a64:";
    for (int shift = 60; shift >= 0; shift -= 4) result.push_back(digits[(value >> shift) & 0x0fU]);
    return result;
}

} // namespace

SkiaHeadlessBackend::SkiaHeadlessBackend(HeadlessRasterConfig config) : _config(config) {}

BackendSubmissionResult SkiaHeadlessBackend::submit(const FramePlan& plan) {
    _lastIssue = HeadlessSubmissionIssue::kNone;
    auto reject = [&](HeadlessSubmissionIssue issue, std::string message) {
        _lastIssue = issue;
        return BackendSubmissionResult::rejected(std::move(message));
    };
    if (_config.width == 0U || _config.height == 0U) return reject(HeadlessSubmissionIssue::kInvalidConfiguration, "invalid raster dimensions");
    const std::uint64_t byteCount = static_cast<std::uint64_t>(_config.width) * _config.height * 4U;
    if (byteCount > std::numeric_limits<std::size_t>::max() || byteCount > 512ULL * 1024ULL * 1024ULL) return reject(HeadlessSubmissionIssue::kOverflow, "raster allocation overflow");
    if (_config.width != 256U || _config.height != 256U) return reject(HeadlessSubmissionIssue::kInvalidConfiguration, "headless raster must be 256x256");
    if (!(plan.referenceDrawList.frame == plan.frame)) return reject(HeadlessSubmissionIssue::kPlanIdentityMismatch, "frame identity mismatch");
    if (plan.referenceDrawList.entries.empty()) return reject(HeadlessSubmissionIssue::kEmptyPlan, "empty reference draw list");
    if (!validAffine(plan.referenceDrawList.worldToView) || !validRect(plan.referenceDrawList.viewportClip)) return reject(HeadlessSubmissionIssue::kUnsupportedGeometry, "non-integral transform or clip");
    for (const auto& entry : plan.referenceDrawList.entries) {
        if (!validRect(entry.record.visualBounds)) return reject(HeadlessSubmissionIssue::kUnsupportedGeometry, "non-integral visual bounds");
        const WorldToViewAffine recordTransform{entry.record.transform.a, entry.record.transform.b, entry.record.transform.c, entry.record.transform.d, entry.record.transform.tx, entry.record.transform.ty};
        if (!scalarRange(recordTransform.a) || !scalarRange(recordTransform.b) ||
            !scalarRange(recordTransform.c) || !scalarRange(recordTransform.d) ||
            !scalarRange(recordTransform.tx) || !scalarRange(recordTransform.ty)) {
            return reject(HeadlessSubmissionIssue::kOverflow, "record transform exceeds raster scalar range");
        }
        if (!validAffine(recordTransform)) return reject(HeadlessSubmissionIssue::kUnsupportedGeometry, "non-integral record transform");
        const auto commandIssue = validateCommand(entry);
        if (commandIssue != HeadlessSubmissionIssue::kNone) {
            return reject(commandIssue, commandIssue == HeadlessSubmissionIssue::kOverflow
                ? "command geometry exceeds raster scalar range"
                : "unsupported command geometry");
        }
        for (const auto& mask : entry.record.eraseMasks) {
            const auto* filled = std::get_if<semantic::FilledPathMask>(&mask.geometry);
            if (filled == nullptr || !validatePath(filled->path)) return reject(HeadlessSubmissionIssue::kUnsupportedEraseMask, "unsupported erase mask");
        }
    }
    const SkImageInfo info = SkImageInfo::Make(_config.width, _config.height, kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::Raster(info);
    if (!surface) return reject(HeadlessSubmissionIssue::kRasterFailure, "unable to create raster surface");
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    canvas->save();
    canvas->clipRect(rect(plan.referenceDrawList.viewportClip));
    const SkMatrix worldMatrix = matrix(plan.referenceDrawList.worldToView);
    std::vector<semantic::ObjectKind> kinds;
    kinds.reserve(plan.referenceDrawList.entries.size());
    for (const auto& entry : plan.referenceDrawList.entries) {
        canvas->save();
        canvas->concat(SkMatrix::Concat(matrix(entry.record.transform), worldMatrix));
        drawCommand(*canvas, entry);
        eraseMasks(*canvas, entry);
        canvas->restore();
        kinds.push_back(entry.record.kind);
    }
    canvas->restore();
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(byteCount));
    if (!surface->readPixels(info, rgba.data(), _config.width * 4U, 0, 0)) return reject(HeadlessSubmissionIssue::kRasterFailure, "unable to read raster surface");
    const std::string rgbaDigest = digest(rgba);
    HeadlessRasterObservation next{_config.width, _config.height, std::move(rgba), rgbaDigest, plan.referenceDrawList.digest, std::move(kinds)};
    _observation = std::move(next);
    return BackendSubmissionResult::accepted();
}

} // namespace canvas::render
