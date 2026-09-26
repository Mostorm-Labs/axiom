#include "canvas/render/direct_reference_source.hpp"

#include <bit>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <type_traits>

namespace canvas::render {
namespace {

class CanonicalWriter final {
  public:
    void byte(std::uint8_t value) { bytes_.push_back(value); }
    void boolean(bool value) { byte(value ? 1U : 0U); }
    void u32(std::uint32_t value) {
        for (unsigned shift = 0; shift < 32U; shift += 8U) {
            byte(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void u64(std::uint64_t value) {
        for (unsigned shift = 0; shift < 64U; shift += 8U) {
            byte(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void f32(float value) { u32(std::bit_cast<std::uint32_t>(value)); }
    void f64(double value) { u64(std::bit_cast<std::uint64_t>(value)); }
    void string(const std::string& value) {
        u64(value.size());
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }
    void id(const foundation::ObjectId& value) {
        bytes_.insert(bytes_.end(), value.bytes.begin(), value.bytes.end());
    }
    void rect(const foundation::WorldRect& value) {
        f32(value.left); f32(value.top); f32(value.right); f32(value.bottom);
    }
    void vec2(const semantic::Vec2& value) { f64(value.x); f64(value.y); }
    void color(const semantic::ColorValue& value) {
        f32(value.r); f32(value.g); f32(value.b); f32(value.a);
    }

    [[nodiscard]] std::vector<std::uint8_t> finish() && { return std::move(bytes_); }

  private:
    std::vector<std::uint8_t> bytes_;
};

template <typename T, typename Encoder>
void encodeOptional(CanonicalWriter& out, const std::optional<T>& value, Encoder encode) {
    out.boolean(value.has_value());
    if (value) encode(*value);
}

void encodeGeometry(CanonicalWriter& out, const semantic::VectorPathGeometry& geometry) {
    out.byte(static_cast<std::uint8_t>(geometry.fill_rule));
    out.u64(geometry.commands.size());
    for (const auto& command : geometry.commands) {
        out.byte(static_cast<std::uint8_t>(command.index()));
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, semantic::MoveTo>) {
                out.vec2(value.point);
            } else if constexpr (std::is_same_v<T, semantic::LineTo>) {
                out.vec2(value.end);
            } else if constexpr (std::is_same_v<T, semantic::QuadTo>) {
                out.vec2(value.control); out.vec2(value.end);
            } else if constexpr (std::is_same_v<T, semantic::CubicTo>) {
                out.vec2(value.control1); out.vec2(value.control2); out.vec2(value.end);
            }
        }, command);
    }
}

void encodeTextStyle(CanonicalWriter& out, const semantic::TextStyle& style) {
    encodeOptional(out, style.font_resource_id,
                   [&](const semantic::ResourceId& id) { out.id(id.value); });
    out.f64(style.font_size);
    out.u32(style.weight);
    out.boolean(style.italic);
    out.boolean(style.underline);
    out.color(style.color);
}

void encodeBrush(CanonicalWriter& out, const semantic::BrushDescriptor& brush) {
    out.u32(brush.brush_family_id);
    out.u32(brush.brush_version);
    out.color(brush.color);
    out.f64(brush.nominal_size);
    out.f32(brush.opacity);
    out.boolean(brush.pressure.enabled);
    const auto encodeCurve = [&](const semantic::PiecewiseLinearCurve01& curve) {
        out.u64(curve.points.size());
        for (const auto& point : curve.points) { out.f32(point.x); out.f32(point.y); }
    };
    encodeOptional(out, brush.pressure.size_curve, encodeCurve);
    encodeOptional(out, brush.pressure.opacity_curve, encodeCurve);
    out.boolean(brush.tilt.enabled);
    out.f32(brush.tilt.size_influence);
    out.f32(brush.tilt.angle_influence);
    out.f32(brush.smoothing.amount);
    out.f32(brush.spacing.normalized_spacing);
    out.byte(static_cast<std::uint8_t>(brush.blend_mode));
    encodeOptional(out, brush.texture_resource_id,
                   [&](const semantic::ResourceId& id) { out.id(id.value); });
}

void encodeStroke(CanonicalWriter& out, const semantic::StrokeRecord& stroke) {
    encodeBrush(out, stroke.brush);
    out.u64(stroke.deterministic_seed);
    out.byte(static_cast<std::uint8_t>(stroke.data.index()));
    std::visit([&](const auto& data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, semantic::VectorStrokeData>) {
            out.u64(data.samples.size());
            for (const auto& sample : data.samples) {
                out.vec2(sample.position); out.f32(sample.pressure); out.vec2(sample.tilt);
            }
        } else {
            out.u64(data.dabs.size());
            for (const auto& dab : data.dabs) {
                out.vec2(dab.center); out.f64(dab.size); out.f32(dab.rotation);
                out.f32(dab.opacity);
            }
        }
    }, stroke.data);
}

void encodeEndpoint(CanonicalWriter& out, const semantic::ConnectorEndpoint& endpoint) {
    out.byte(static_cast<std::uint8_t>(endpoint.value.index()));
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, semantic::FreePointEndpoint>) {
            out.vec2(value.point);
        } else {
            out.id(value.target_object_id);
            out.byte(static_cast<std::uint8_t>(value.anchor.index()));
            std::visit([&](const auto& anchor) {
                using A = std::decay_t<decltype(anchor)>;
                if constexpr (std::is_same_v<A, semantic::AutoPerimeterAnchor>) {
                    encodeOptional(out, anchor.hint,
                                   [&](const semantic::Vec2& hint) { out.vec2(hint); });
                } else {
                    out.u32(anchor.port_id);
                }
            }, value.anchor);
        }
    }, endpoint.value);
}

void encodeContent(CanonicalWriter& out, const semantic::ObjectContent& content) {
    out.byte(static_cast<std::uint8_t>(content.index()));
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, semantic::ShapeContent>) {
            out.u32(value.shape_kind); out.f64(value.width); out.f64(value.height);
        } else if constexpr (std::is_same_v<T, semantic::ImageContent>) {
            out.id(value.resource_id.value);
            out.f64(value.intrinsic_width); out.f64(value.intrinsic_height);
            encodeOptional(out, value.source_rect, [&](const semantic::NormalizedRect& rect) {
                out.f64(rect.x); out.f64(rect.y); out.f64(rect.width); out.f64(rect.height);
            });
            out.byte(static_cast<std::uint8_t>(value.content_mode));
            out.f64(value.width); out.f64(value.height);
        } else if constexpr (std::is_same_v<T, semantic::VectorPathContent>) {
            encodeGeometry(out, value.geometry);
        } else if constexpr (std::is_same_v<T, semantic::RichTextContent>) {
            out.u64(value.document.paragraphs.size());
            for (const auto& paragraph : value.document.paragraphs) {
                out.id(paragraph.id);
                out.byte(static_cast<std::uint8_t>(paragraph.style.alignment));
                out.f64(paragraph.style.line_height);
                out.f64(paragraph.style.spacing_before);
                out.f64(paragraph.style.spacing_after);
                out.u64(paragraph.runs.size());
                for (const auto& run : paragraph.runs) {
                    out.string(run.text); encodeTextStyle(out, run.style);
                }
            }
        } else if constexpr (std::is_same_v<T, semantic::VectorStrokeContent> ||
                             std::is_same_v<T, semantic::DabStrokeContent>) {
            encodeStroke(out, value.stroke);
        } else if constexpr (std::is_same_v<T, semantic::BrushStrokeContent>) {
            out.u32(value.stroke.snapshot.snapshot_version);
            out.u32(value.stroke.snapshot.package_revision);
            out.u64(value.stroke.snapshot.seed);
            out.u64(value.stroke.confirmed_samples.size());
            for (const auto& sample : value.stroke.confirmed_samples) {
                out.vec2(sample.position); encodeOptional(out, sample.pressure,
                    [&](const double pressure) { out.f64(pressure); });
            }
            out.u32(value.stroke.vector_output.fill_rule);
            out.boolean(value.stroke.vector_output.closed);
            out.u64(value.stroke.vector_output.outline.size());
            for (const auto& point : value.stroke.vector_output.outline) out.vec2(point);
        } else if constexpr (std::is_same_v<T, semantic::ConnectorContent>) {
            encodeEndpoint(out, value.start); encodeEndpoint(out, value.end);
            out.byte(static_cast<std::uint8_t>(value.routing));
        } else if constexpr (std::is_same_v<T, semantic::StickyContent>) {
            out.f64(value.width); out.f64(value.height);
        }
    }, content);
}

void encodePropertyValue(CanonicalWriter& out, const semantic::PropertyValue& property) {
    out.byte(static_cast<std::uint8_t>(property.index()));
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            out.boolean(value);
        } else if constexpr (std::is_same_v<T, float>) {
            out.f32(value);
        } else if constexpr (std::is_same_v<T, semantic::ColorValue>) {
            out.color(value);
        } else if constexpr (std::is_same_v<T, semantic::FillStyleValue>) {
            out.byte(static_cast<std::uint8_t>(value.index()));
            if (const auto* fill = std::get_if<semantic::SolidFill>(&value)) out.color(fill->color);
        } else if constexpr (std::is_same_v<T, semantic::StrokeStyleValue>) {
            out.byte(static_cast<std::uint8_t>(value.index()));
            if (const auto* stroke = std::get_if<semantic::SolidStroke>(&value)) {
                out.color(stroke->color); out.f64(stroke->width);
                out.byte(static_cast<std::uint8_t>(stroke->cap));
                out.byte(static_cast<std::uint8_t>(stroke->join.index()));
                if (const auto* join = std::get_if<semantic::MiterJoin>(&stroke->join)) {
                    out.f64(join->limit);
                }
                out.byte(static_cast<std::uint8_t>(stroke->dash.index()));
                if (const auto* dash = std::get_if<semantic::DashPattern>(&stroke->dash)) {
                    out.u64(dash->segments.size());
                    for (double segment : dash->segments) out.f64(segment);
                    out.f64(dash->offset);
                }
            }
        } else {
            out.byte(static_cast<std::uint8_t>(value));
        }
    }, property);
}

void encodeEraseMask(CanonicalWriter& out, const semantic::EraseMaskRecord& mask) {
    out.id(mask.id);
    out.byte(static_cast<std::uint8_t>(mask.geometry.index()));
    std::visit([&](const auto& geometry) {
        using T = std::decay_t<decltype(geometry)>;
        if constexpr (std::is_same_v<T, semantic::SweptCircleMask>) {
            out.u64(geometry.segments.size());
            for (const auto& segment : geometry.segments) {
                out.vec2(segment.p0.position); out.f64(segment.p0.radius);
                out.vec2(segment.p1.position); out.f64(segment.p1.radius);
                out.vec2(segment.control1); out.vec2(segment.control2);
            }
        } else {
            encodeGeometry(out, geometry.path);
        }
    }, mask.geometry);
}

void encodeRecord(CanonicalWriter& out, const RuntimeSceneRecord& record) {
    out.id(record.objectId);
    out.byte(static_cast<std::uint8_t>(record.kind));
    out.u32(record.kindVersion);
    encodeOptional(out, record.placement.parent_id,
                   [&](const semantic::ObjectId& id) { out.id(id); });
    const auto order = record.placement.order_key.bytes();
    out.u64(order.size());
    for (std::uint8_t byte : order) out.byte(byte);
    out.f64(record.transform.a); out.f64(record.transform.b);
    out.f64(record.transform.c); out.f64(record.transform.d);
    out.f64(record.transform.tx); out.f64(record.transform.ty);
    out.u64(record.properties.entries.size());
    for (const auto& property : record.properties.entries) {
        out.u32(property.field_id); encodePropertyValue(out, property.value);
    }
    encodeContent(out, record.content);
    out.u64(record.eraseMasks.size());
    for (const auto& mask : record.eraseMasks) encodeEraseMask(out, mask);
    out.rect(record.geometryBounds); out.rect(record.visualBounds); out.rect(record.worldBounds);
    out.string(record.referenceGeometryDigest);
    out.u64(record.directDependencies.size());
    for (const auto& dependency : record.directDependencies) out.id(dependency);
}

void encodeFrame(CanonicalWriter& out, const FrameState& frame) {
    out.u64(frame.viewId.value());
    out.f32(frame.camera.worldCenter.x); out.f32(frame.camera.worldCenter.y);
    out.f32(frame.camera.zoom); out.f32(frame.camera.rotationRadians);
    out.u64(frame.camera.generation.value());
    out.rect(frame.worldViewport);
    out.f32(frame.metrics.logicalWidth); out.f32(frame.metrics.logicalHeight);
    out.u32(frame.metrics.physicalWidth); out.u32(frame.metrics.physicalHeight);
    out.f32(frame.metrics.devicePixelRatio); out.f32(frame.metrics.displayScale);
    out.u64(frame.sceneGeneration.value()); out.u64(frame.sceneReadToken.value());
    out.u64(frame.surfaceGeneration.value()); out.u64(frame.metricsGeneration.value());
    out.u64(frame.frameId.value());
}

std::vector<std::uint8_t> canonicalBytes(const ReferenceDrawList& list) {
    CanonicalWriter out;
    out.string("AXIOM_REFERENCE_DRAW_LIST");
    out.u32(list.canonicalEncodingVersion);
    out.byte(static_cast<std::uint8_t>(list.canonicalByteOrder));
    encodeFrame(out, list.frame);
    out.rect(list.queryWorldRect);
    out.u64(list.candidatesExamined); out.u64(list.visibleRecords);
    out.f64(list.worldToView.a); out.f64(list.worldToView.b);
    out.f64(list.worldToView.c); out.f64(list.worldToView.d);
    out.f64(list.worldToView.tx); out.f64(list.worldToView.ty);
    out.rect(list.viewportClip);
    out.u64(list.entries.size());
    for (const auto& entry : list.entries) {
        encodeRecord(out, entry.record);
        out.boolean(entry.contributesPixels);
        out.byte(static_cast<std::uint8_t>(entry.command.index()));
    }
    out.u64(list.diagnostics.visibleIdsProcessed);
    out.u64(list.diagnostics.runtimeSceneFindLookups);
    out.u64(list.diagnostics.runtimeSceneRecordIterations);
    out.u64(list.diagnostics.semanticObjectIterations);
    return std::move(out).finish();
}

std::string stableDigest(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t digest = 1469598103934665603ULL;
    for (std::uint8_t byte : bytes) {
        digest ^= byte;
        digest *= 1099511628211ULL;
    }
    std::ostringstream text;
    text << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << digest;
    return text.str();
}

WorldToViewAffine worldToView(const FrameState& frame) {
    const double zoom = frame.camera.zoom;
    const double cosine = std::cos(static_cast<double>(frame.camera.rotationRadians));
    const double sine = std::sin(static_cast<double>(frame.camera.rotationRadians));
    const double a = zoom * cosine;
    const double b = -zoom * sine;
    const double c = zoom * sine;
    const double d = zoom * cosine;
    const double centerX = static_cast<double>(frame.metrics.logicalWidth) / 2.0;
    const double centerY = static_cast<double>(frame.metrics.logicalHeight) / 2.0;
    return WorldToViewAffine{
        a, b, c, d,
        centerX - a * frame.camera.worldCenter.x - c * frame.camera.worldCenter.y,
        centerY - b * frame.camera.worldCenter.x - d * frame.camera.worldCenter.y,
    };
}

foundation::Result<ReferenceCommand> commandFor(const RuntimeSceneRecord& record) {
    const auto mismatch = [] {
        return foundation::Result<ReferenceCommand>::failure(
            {foundation::ErrorCode::kInvalidRecord,
             "RuntimeScene object kind and content branch do not match"});
    };
    switch (record.kind) {
    case semantic::ObjectKind::kShape:
        if (const auto* value = std::get_if<semantic::ShapeContent>(&record.content))
            return foundation::Result<ReferenceCommand>::success(ShapeReferenceCommand{*value});
        break;
    case semantic::ObjectKind::kImage:
        if (const auto* value = std::get_if<semantic::ImageContent>(&record.content))
            return foundation::Result<ReferenceCommand>::success(ImageReferenceCommand{*value});
        break;
    case semantic::ObjectKind::kVectorPath:
        if (const auto* value = std::get_if<semantic::VectorPathContent>(&record.content))
            return foundation::Result<ReferenceCommand>::success(VectorPathReferenceCommand{*value});
        break;
    case semantic::ObjectKind::kRichText:
        if (const auto* value = std::get_if<semantic::RichTextContent>(&record.content))
            return foundation::Result<ReferenceCommand>::success(RichTextReferenceCommand{*value});
        break;
    case semantic::ObjectKind::kVectorStroke:
        if (record.kindVersion == 2U) {
            if (const auto* value = std::get_if<semantic::BrushStrokeContent>(&record.content))
                return foundation::Result<ReferenceCommand>::success(BrushStrokeReferenceCommand{*value});
            break;
        }
        if (const auto* value = std::get_if<semantic::VectorStrokeContent>(&record.content))
            return foundation::Result<ReferenceCommand>::success(VectorStrokeReferenceCommand{*value});
        break;
    case semantic::ObjectKind::kDabStroke:
        if (const auto* value = std::get_if<semantic::DabStrokeContent>(&record.content))
            return foundation::Result<ReferenceCommand>::success(DabStrokeReferenceCommand{*value});
        break;
    case semantic::ObjectKind::kConnector:
        if (const auto* value = std::get_if<semantic::ConnectorContent>(&record.content))
            return foundation::Result<ReferenceCommand>::success(ConnectorReferenceCommand{*value});
        break;
    case semantic::ObjectKind::kSticky:
        if (const auto* value = std::get_if<semantic::StickyContent>(&record.content))
            return foundation::Result<ReferenceCommand>::success(StickyReferenceCommand{*value});
        break;
    case semantic::ObjectKind::kGroup:
        if (std::holds_alternative<semantic::GroupContent>(record.content))
            return foundation::Result<ReferenceCommand>::success(GroupReferenceCommand{});
        break;
    }
    return mismatch();
}

} // namespace

foundation::Result<ReferenceDrawList> DirectReferenceSource::build(
    const FrameState& frame,
    const VisibilityResult& visibility,
    const RuntimeScene& runtimeScene) {
    if (frame.sceneGeneration != visibility.sceneGeneration ||
        frame.sceneReadToken != visibility.sceneReadToken ||
        frame.sceneGeneration != runtimeScene.generation()) {
        return foundation::Result<ReferenceDrawList>::failure(
            {foundation::ErrorCode::kInvalidRevision,
             "Frame, visibility, and RuntimeScene identities do not match"});
    }
    if (visibility.visibleRecords != visibility.backToFront.size()) {
        return foundation::Result<ReferenceDrawList>::failure(
            {foundation::ErrorCode::kInvalidArgument,
             "Visibility result count does not match its ordered IDs"});
    }

    ReferenceDrawList list{
        .frame = frame,
        .queryWorldRect = visibility.queryWorldRect,
        .candidatesExamined = visibility.candidatesExamined,
        .visibleRecords = visibility.visibleRecords,
        .worldToView = worldToView(frame),
        .viewportClip = frame.worldViewport,
        .entries = {},
        .diagnostics = {},
        .canonicalEncodingVersion = ReferenceDrawList::kCanonicalEncodingVersion,
        .canonicalByteOrder = CanonicalByteOrder::kLittleEndian,
        .canonicalBytes = {},
        .digest = {},
    };
    list.entries.reserve(visibility.backToFront.size());
    for (const foundation::ObjectId id : visibility.backToFront) {
        ++list.diagnostics.visibleIdsProcessed;
        ++list.diagnostics.runtimeSceneFindLookups;
        const RuntimeSceneRecord* record = runtimeScene.find(id);
        if (record == nullptr) {
            return foundation::Result<ReferenceDrawList>::failure(
                {foundation::ErrorCode::kMissingObject,
                 "Visibility references an object missing from RuntimeScene"});
        }
        auto command = commandFor(*record);
        if (!command) return foundation::Result<ReferenceDrawList>::failure(command.error());
        list.entries.push_back(ReferenceTraversalEntry{
            .record = *record,
            .contributesPixels = record->kind != semantic::ObjectKind::kGroup,
            .command = std::move(command.value()),
        });
    }
    list.canonicalBytes = canonicalBytes(list);
    list.digest = stableDigest(list.canonicalBytes);
    return foundation::Result<ReferenceDrawList>::success(std::move(list));
}

} // namespace canvas::render
