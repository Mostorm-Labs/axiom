#include "g1_06_projection.hpp"

#include <nlohmann/json.hpp>

#include <bit>
#include <cstdint>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace canvas::verification::g1_06 {
namespace {

using namespace canvas::semantic;
using Json = nlohmann::ordered_json;

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};
template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

template <typename Integer>
std::string fixedHex(Integer value, std::size_t width) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0') << std::setw(static_cast<int>(width))
        << value;
    return out.str();
}

std::string byteHex(std::span<const std::uint8_t> bytes, const char* prefix) {
    std::ostringstream out;
    out << prefix << std::hex << std::nouppercase << std::setfill('0');
    for (const std::uint8_t byte : bytes) {
        out << std::setw(2) << static_cast<unsigned>(byte);
    }
    return out.str();
}

std::string id128(const ObjectId& value) {
    return byteHex(value.bytes, "id128:");
}

std::string id128(const ResourceId& value) {
    return id128(value.value);
}

std::string id128(const ProjectionDocumentId& value) {
    return byteHex(value.bytes, "id128:");
}

std::string bytes(const OrderKey& value) {
    return byteHex(value.bytes(), "hex:");
}

std::string f32(float value) {
    return "f32:" + fixedHex(std::bit_cast<std::uint32_t>(value), 8U);
}

std::string f64(double value) {
    return "f64:" + fixedHex(std::bit_cast<std::uint64_t>(value), 16U);
}

std::string u64(std::uint64_t value) {
    return "u64:" + fixedHex(value, 16U);
}

template <typename Enum>
std::uint32_t enumValue(Enum value) {
    return static_cast<std::uint32_t>(value);
}

Json writeVec2(const Vec2& value) {
    Json result = Json::object();
    result["x"] = f64(value.x);
    result["y"] = f64(value.y);
    return result;
}

Json writeNormalizedRect(const NormalizedRect& value) {
    Json result = Json::object();
    result["x"] = f64(value.x);
    result["y"] = f64(value.y);
    result["width"] = f64(value.width);
    result["height"] = f64(value.height);
    return result;
}

Json writeTransform(const Transform2D& value) {
    Json result = Json::object();
    result["a"] = f64(value.a);
    result["b"] = f64(value.b);
    result["c"] = f64(value.c);
    result["d"] = f64(value.d);
    result["tx"] = f64(value.tx);
    result["ty"] = f64(value.ty);
    return result;
}

Json writeColor(const ColorValue& value) {
    Json result = Json::object();
    result["r"] = f32(value.r);
    result["g"] = f32(value.g);
    result["b"] = f32(value.b);
    result["a"] = f32(value.a);
    return result;
}

Json writeFillStyle(const FillStyleValue& value) {
    return std::visit(Overloaded{
        [](const NoFill&) {
            Json result = Json::object();
            result["none"] = Json::object();
            return result;
        },
        [](const SolidFill& fill) {
            Json solid = Json::object();
            solid["color"] = writeColor(fill.color);
            Json result = Json::object();
            result["solid"] = std::move(solid);
            return result;
        }}, value);
}

Json writeStrokeJoin(const StrokeJoin& value) {
    return std::visit(Overloaded{
        [](const MiterJoin& join) {
            Json miter = Json::object();
            miter["limit"] = f64(join.limit);
            Json result = Json::object();
            result["miter"] = std::move(miter);
            return result;
        },
        [](const RoundJoin&) {
            Json result = Json::object();
            result["round"] = Json::object();
            return result;
        },
        [](const BevelJoin&) {
            Json result = Json::object();
            result["bevel"] = Json::object();
            return result;
        }}, value);
}

Json writeStrokeDash(const StrokeDash& value) {
    return std::visit(Overloaded{
        [](const SolidDash&) {
            Json result = Json::object();
            result["solid"] = Json::object();
            return result;
        },
        [](const DashPattern& dash) {
            Json pattern = Json::object();
            pattern["segments"] = Json::array();
            for (const double segment : dash.segments) {
                pattern["segments"].push_back(f64(segment));
            }
            pattern["offset"] = f64(dash.offset);
            Json result = Json::object();
            result["pattern"] = std::move(pattern);
            return result;
        }}, value);
}

Json writeStrokeStyle(const StrokeStyleValue& value) {
    return std::visit(Overloaded{
        [](const NoStroke&) {
            Json result = Json::object();
            result["none"] = Json::object();
            return result;
        },
        [](const SolidStroke& stroke) {
            Json solid = Json::object();
            solid["color"] = writeColor(stroke.color);
            solid["width"] = f64(stroke.width);
            solid["cap"] = enumValue(stroke.cap);
            solid["join"] = writeStrokeJoin(stroke.join);
            solid["dash"] = writeStrokeDash(stroke.dash);
            Json result = Json::object();
            result["solid"] = std::move(solid);
            return result;
        }}, value);
}

Json writePropertyValue(const PropertyValue& value) {
    return std::visit(Overloaded{
        [](bool item) {
            Json result = Json::object();
            result["bool_value"] = item;
            return result;
        },
        [](float item) {
            Json result = Json::object();
            result["f32_value"] = f32(item);
            return result;
        },
        [](const ColorValue& item) {
            Json result = Json::object();
            result["color_value"] = writeColor(item);
            return result;
        },
        [](const FillStyleValue& item) {
            Json result = Json::object();
            result["fill_style"] = writeFillStyle(item);
            return result;
        },
        [](const StrokeStyleValue& item) {
            Json result = Json::object();
            result["stroke_style"] = writeStrokeStyle(item);
            return result;
        },
        [](BlendModeValue item) {
            Json result = Json::object();
            result["blend_mode"] = enumValue(item);
            return result;
        },
        [](ConnectorDecorationValue item) {
            Json result = Json::object();
            result["connector_decoration"] = enumValue(item);
            return result;
        }}, value);
}

Json writePropertyBag(const PropertyBag& bag) {
    Json result = Json::object();
    result["entries"] = Json::array();
    for (const PropertyEntry& entry : bag.entries) {
        Json item = Json::object();
        item["field_id"] = entry.field_id;
        item["value"] = writePropertyValue(entry.value);
        result["entries"].push_back(std::move(item));
    }
    return result;
}

Json writePathCommand(const PathCommand& value) {
    return std::visit(Overloaded{
        [](const MoveTo& command) {
            Json message = Json::object();
            message["point"] = writeVec2(command.point);
            Json result = Json::object();
            result["move_to"] = std::move(message);
            return result;
        },
        [](const LineTo& command) {
            Json message = Json::object();
            message["end"] = writeVec2(command.end);
            Json result = Json::object();
            result["line_to"] = std::move(message);
            return result;
        },
        [](const QuadTo& command) {
            Json message = Json::object();
            message["control"] = writeVec2(command.control);
            message["end"] = writeVec2(command.end);
            Json result = Json::object();
            result["quad_to"] = std::move(message);
            return result;
        },
        [](const CubicTo& command) {
            Json message = Json::object();
            message["control1"] = writeVec2(command.control1);
            message["control2"] = writeVec2(command.control2);
            message["end"] = writeVec2(command.end);
            Json result = Json::object();
            result["cubic_to"] = std::move(message);
            return result;
        },
        [](const ClosePath&) {
            Json result = Json::object();
            result["close_path"] = Json::object();
            return result;
        }}, value);
}

Json writeVectorPathGeometry(const VectorPathGeometry& value) {
    Json result = Json::object();
    result["fill_rule"] = enumValue(value.fill_rule);
    result["commands"] = Json::array();
    for (const PathCommand& command : value.commands) {
        result["commands"].push_back(writePathCommand(command));
    }
    return result;
}

Json writeCurve(const PiecewiseLinearCurve01& value) {
    Json result = Json::object();
    result["points"] = Json::array();
    for (const CurvePoint01& point : value.points) {
        Json item = Json::object();
        item["x"] = f32(point.x);
        item["y"] = f32(point.y);
        result["points"].push_back(std::move(item));
    }
    return result;
}

Json writeBrush(const BrushDescriptor& value) {
    Json result = Json::object();
    result["brush_family_id"] = value.brush_family_id;
    result["brush_version"] = value.brush_version;
    result["color"] = writeColor(value.color);
    result["nominal_size"] = f64(value.nominal_size);
    result["opacity"] = f32(value.opacity);

    Json pressure = Json::object();
    pressure["enabled"] = value.pressure.enabled;
    if (value.pressure.size_curve.has_value()) {
        pressure["size_curve"] = writeCurve(*value.pressure.size_curve);
    }
    if (value.pressure.opacity_curve.has_value()) {
        pressure["opacity_curve"] = writeCurve(*value.pressure.opacity_curve);
    }
    result["pressure"] = std::move(pressure);

    Json tilt = Json::object();
    tilt["enabled"] = value.tilt.enabled;
    tilt["size_influence"] = f32(value.tilt.size_influence);
    tilt["angle_influence"] = f32(value.tilt.angle_influence);
    result["tilt"] = std::move(tilt);

    Json smoothing = Json::object();
    smoothing["amount"] = f32(value.smoothing.amount);
    result["smoothing"] = std::move(smoothing);
    Json spacing = Json::object();
    spacing["normalized_spacing"] = f32(value.spacing.normalized_spacing);
    result["spacing"] = std::move(spacing);
    if (value.texture_resource_id.has_value()) {
        result["texture_resource_id"] = id128(*value.texture_resource_id);
    }
    result["blend_mode"] = enumValue(value.blend_mode);
    return result;
}

Json writeStrokeSample(const StrokeSample& value) {
    Json result = Json::object();
    result["position"] = writeVec2(value.position);
    result["pressure"] = f32(value.pressure);
    result["tilt"] = writeVec2(value.tilt);
    return result;
}

Json writeStroke(const StrokeRecord& value) {
    Json result = Json::object();
    result["brush"] = writeBrush(value.brush);
    result["deterministic_seed"] = u64(value.deterministic_seed);
    std::visit(Overloaded{
        [&](const VectorStrokeData& vector) {
            Json message = Json::object();
            message["samples"] = Json::array();
            for (const StrokeSample& sample : vector.samples) {
                message["samples"].push_back(writeStrokeSample(sample));
            }
            result["vector"] = std::move(message);
        },
        [&](const DabStrokeData& dab) {
            Json message = Json::object();
            message["dabs"] = Json::array();
            for (const DabInstance& instance : dab.dabs) {
                Json item = Json::object();
                item["center"] = writeVec2(instance.center);
                item["size"] = f64(instance.size);
                item["rotation"] = f32(instance.rotation);
                item["opacity"] = f32(instance.opacity);
                message["dabs"].push_back(std::move(item));
            }
            result["dab"] = std::move(message);
        }}, value.data);
    return result;
}

Json writeTextStyle(const TextStyle& value) {
    Json result = Json::object();
    if (value.font_resource_id.has_value()) {
        result["font_resource_id"] = id128(*value.font_resource_id);
    }
    result["font_size"] = f64(value.font_size);
    result["weight"] = value.weight;
    result["italic"] = value.italic;
    result["underline"] = value.underline;
    result["color"] = writeColor(value.color);
    return result;
}

Json writeParagraphStyle(const ParagraphStyle& value) {
    Json result = Json::object();
    result["alignment"] = enumValue(value.alignment);
    result["line_height"] = f64(value.line_height);
    result["spacing_before"] = f64(value.spacing_before);
    result["spacing_after"] = f64(value.spacing_after);
    return result;
}

Json writeRichText(const RichTextContent& value) {
    Json document = Json::object();
    document["paragraphs"] = Json::array();
    for (const Paragraph& paragraph : value.document.paragraphs) {
        Json item = Json::object();
        item["paragraph_id"] = id128(paragraph.id);
        item["style"] = writeParagraphStyle(paragraph.style);
        item["runs"] = Json::array();
        for (const TextRun& run : paragraph.runs) {
            Json run_json = Json::object();
            run_json["text"] = run.text;
            run_json["style"] = writeTextStyle(run.style);
            item["runs"].push_back(std::move(run_json));
        }
        document["paragraphs"].push_back(std::move(item));
    }
    Json result = Json::object();
    result["document"] = std::move(document);
    return result;
}

Json writeAnchor(const AnchorRef& value) {
    return std::visit(Overloaded{
        [](const AutoPerimeterAnchor& anchor) {
            Json message = Json::object();
            if (anchor.hint.has_value()) {
                message["hint"] = writeVec2(*anchor.hint);
            }
            Json result = Json::object();
            result["auto_perimeter"] = std::move(message);
            return result;
        },
        [](const StablePortAnchor& anchor) {
            Json message = Json::object();
            message["port_id"] = anchor.port_id;
            Json result = Json::object();
            result["stable_port"] = std::move(message);
            return result;
        }}, value);
}

Json writeEndpoint(const ConnectorEndpoint& value) {
    return std::visit(Overloaded{
        [](const FreePointEndpoint& endpoint) {
            Json message = Json::object();
            message["point"] = writeVec2(endpoint.point);
            Json result = Json::object();
            result["free_point"] = std::move(message);
            return result;
        },
        [](const AttachedEndpoint& endpoint) {
            Json message = Json::object();
            message["target_object_id"] = id128(endpoint.target_object_id);
            message["anchor"] = writeAnchor(endpoint.anchor);
            Json result = Json::object();
            result["attached"] = std::move(message);
            return result;
        }}, value.value);
}

Json writeEraseKnot(const EraseKnot& value) {
    Json result = Json::object();
    result["position"] = writeVec2(value.position);
    result["radius"] = f64(value.radius);
    return result;
}

Json writeEraseGeometry(const EraseMaskGeometry& value) {
    return std::visit(Overloaded{
        [](const SweptCircleMask& mask) {
            Json message = Json::object();
            message["segments"] = Json::array();
            for (const EraseCubicSegment& segment : mask.segments) {
                Json item = Json::object();
                item["p0"] = writeEraseKnot(segment.p0);
                item["p1"] = writeEraseKnot(segment.p1);
                item["control1"] = writeVec2(segment.control1);
                item["control2"] = writeVec2(segment.control2);
                message["segments"].push_back(std::move(item));
            }
            Json result = Json::object();
            result["swept_circle"] = std::move(message);
            return result;
        },
        [](const FilledPathMask& mask) {
            Json message = Json::object();
            message["path"] = writeVectorPathGeometry(mask.path);
            Json result = Json::object();
            result["filled_path"] = std::move(message);
            return result;
        }}, value);
}

Json writeContent(const ObjectContent& value) {
    return std::visit(Overloaded{
        [](const ShapeContent& content) {
            Json message = Json::object();
            message["shape_kind"] = content.shape_kind;
            message["width"] = f64(content.width);
            message["height"] = f64(content.height);
            Json result = Json::object(); result["shape"] = std::move(message); return result;
        },
        [](const ImageContent& content) {
            Json message = Json::object();
            message["resource_id"] = id128(content.resource_id);
            message["intrinsic_width"] = f64(content.intrinsic_width);
            message["intrinsic_height"] = f64(content.intrinsic_height);
            if (content.source_rect.has_value()) message["source_rect"] = writeNormalizedRect(*content.source_rect);
            message["content_mode"] = enumValue(content.content_mode);
            message["width"] = f64(content.width);
            message["height"] = f64(content.height);
            Json result = Json::object(); result["image"] = std::move(message); return result;
        },
        [](const VectorPathContent& content) {
            Json message = Json::object(); message["geometry"] = writeVectorPathGeometry(content.geometry);
            Json result = Json::object(); result["vector_path"] = std::move(message); return result;
        },
        [](const RichTextContent& content) {
            Json result = Json::object(); result["rich_text"] = writeRichText(content); return result;
        },
        [](const VectorStrokeContent& content) {
            Json message = Json::object(); message["stroke"] = writeStroke(content.stroke);
            Json result = Json::object(); result["vector_stroke"] = std::move(message); return result;
        },
        [](const DabStrokeContent& content) {
            Json message = Json::object(); message["stroke"] = writeStroke(content.stroke);
            Json result = Json::object(); result["dab_stroke"] = std::move(message); return result;
        },
        [](const ConnectorContent& content) {
            Json message = Json::object();
            message["start"] = writeEndpoint(content.start);
            message["end"] = writeEndpoint(content.end);
            message["routing"] = enumValue(content.routing);
            Json result = Json::object(); result["connector"] = std::move(message); return result;
        },
        [](const StickyContent& content) {
            Json message = Json::object(); message["width"] = f64(content.width); message["height"] = f64(content.height);
            Json result = Json::object(); result["sticky"] = std::move(message); return result;
        },
        [](const GroupContent&) {
            Json result = Json::object(); result["group"] = Json::object(); return result;
        }}, value);
}

Json writeObject(const ObjectRecord& value) {
    Json result = Json::object();
    result["id"] = id128(value.id);
    result["kind_id"] = enumValue(value.kind);
    result["kind_version"] = value.kind_version;
    Json placement = Json::object();
    if (value.placement.parent_id.has_value()) placement["parent_id"] = id128(*value.placement.parent_id);
    placement["order_key"] = bytes(value.placement.order_key);
    result["placement"] = std::move(placement);
    result["transform"] = writeTransform(value.transform);
    result["properties"] = writePropertyBag(value.properties);
    result["content"] = writeContent(value.content);
    result["erase_masks"] = Json::array();
    for (const EraseMaskRecord& mask : value.erase_masks) {
        Json item = Json::object();
        item["mask_id"] = id128(mask.id);
        item["geometry"] = writeEraseGeometry(mask.geometry);
        result["erase_masks"].push_back(std::move(item));
    }
    return result;
}

} // namespace

SemanticProjection projectDocument(
    ProjectionDocumentId document_id,
    std::uint32_t schema_version,
    const canvas::semantic::ObjectStore& store) {
    return SemanticProjection{document_id, schema_version, store.allObjects()};
}

std::string writeCanonicalProjectionJson(const SemanticProjection& projection) {
    Json value = Json::object();
    value["document_id"] = id128(projection.document_id);
    value["schema_version"] = projection.schema_version;
    value["objects"] = Json::array();
    for (const ObjectRecord& object : projection.objects) {
        value["objects"].push_back(writeObject(object));
    }

    Json envelope = Json::object();
    envelope["format"] = "axiom-verification-projection-v1";
    envelope["formatVersion"] = 1;
    envelope["semanticSchemaVersion"] = 1;
    envelope["rootType"] = "auditoryworks.axiom.v1.DocumentSnapshot";
    envelope["form"] = "CANONICAL";
    envelope["value"] = std::move(value);
    return envelope.dump(2) + '\n';
}

} // namespace canvas::verification::g1_06
