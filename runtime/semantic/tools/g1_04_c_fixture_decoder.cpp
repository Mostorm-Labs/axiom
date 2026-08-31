#include "g1_04_c_fixture_decoder.hpp"

#include <nlohmann/json.hpp>

#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>

namespace canvas::verification::g1_04_c {
namespace {
using json = nlohmann::json;
using namespace canvas::semantic;

std::string failMessage(const std::string& path, const std::string& message) {
    return path + ": " + message;
}

bool isLowerHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

bool parseId(const json& value, ObjectId& out, std::string& error, const std::string& path) {
    if (!value.is_string()) { error = failMessage(path, "expected 32-character hex id"); return false; }
    const auto text = value.get<std::string>();
    if (text.size() != 32U) { error = failMessage(path, "invalid id length"); return false; }
    for (char c : text) if (!isLowerHex(c)) { error = failMessage(path, "invalid id hex"); return false; }
    auto nibble = [](char c) -> std::uint8_t { return c <= '9' ? static_cast<std::uint8_t>(c - '0') : static_cast<std::uint8_t>(c - 'a' + 10); };
    for (std::size_t i = 0; i < 16U; ++i) out.bytes[i] = static_cast<std::uint8_t>((nibble(text[2U * i]) << 4U) | nibble(text[2U * i + 1U]));
    return true;
}

bool parseU32(const json& value, std::uint32_t& out, std::string& error, const std::string& path) {
    if (!value.is_number_unsigned() && !value.is_number_integer()) { error = failMessage(path, "expected unsigned integer"); return false; }
    const auto n = value.get<std::int64_t>();
    if (n < 0 || static_cast<std::uint64_t>(n) > std::numeric_limits<std::uint32_t>::max()) { error = failMessage(path, "integer out of range"); return false; }
    out = static_cast<std::uint32_t>(n); return true;
}

bool parseDouble(const json& value, double& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (value.is_number()) { out = value.get<double>(); return true; }
    if (!value.is_string()) { error = failMessage(path, "expected numeric value or f64 carrier"); return false; }
    const auto text = value.get<std::string>();
    constexpr std::string_view prefix = "f64:";
    if (text.size() != prefix.size() + 16U || text.compare(0, prefix.size(), prefix) != 0) { error = failMessage(path, "malformed f64 carrier"); return false; }
    std::uint64_t raw = 0;
    for (std::size_t i = prefix.size(); i < text.size(); ++i) {
        const char c = text[i]; if (!isLowerHex(c)) { error = failMessage(path, "f64 carrier must use lowercase hex"); return false; }
        const std::uint64_t nibble = c <= '9' ? static_cast<std::uint64_t>(c - '0') : static_cast<std::uint64_t>(c - 'a' + 10);
        raw = (raw << 4U) | nibble;
    }
    out = std::bit_cast<double>(raw); bits.push_back(raw); return true;
}

bool parseFloat(const json& value, float& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    double d = 0.0; if (!parseDouble(value, d, bits, error, path)) return false; out = static_cast<float>(d); return true;
}

bool parseVec(const json& value, Vec2& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (!value.is_array() || value.size() != 2U) { error = failMessage(path, "expected [x,y]"); return false; }
    return parseDouble(value[0], out.x, bits, error, path + "[0]") && parseDouble(value[1], out.y, bits, error, path + "[1]");
}

bool parseOrder(const json& value, OrderKey& out, std::string& error, const std::string& path) {
    if (!value.is_array()) { error = failMessage(path, "expected order-key array"); return false; }
    std::vector<std::uint8_t> bytes; bytes.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) { std::uint32_t n = 0; if (!parseU32(value[i], n, error, path + "[" + std::to_string(i) + "]") || n > 255U) return false; bytes.push_back(static_cast<std::uint8_t>(n)); }
    out = OrderKey(std::move(bytes)); return true;
}

bool parsePlacement(const json& value, Placement& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (!value.is_object()) { error = failMessage(path, "expected placement object"); return false; }
    if (value.contains("parent_id") && !value["parent_id"].is_null()) { ObjectId id; if (!parseId(value["parent_id"], id, error, path + ".parent_id")) return false; out.parent_id = id; }
    return parseOrder(value.value("order_key", json::array()), out.order_key, error, path + ".order_key");
}

bool parseColor(const json& value, ColorValue& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (!value.is_array() || value.size() != 4U) { error = failMessage(path, "expected RGBA array"); return false; }
    return parseFloat(value[0], out.r, bits, error, path + "[0]") && parseFloat(value[1], out.g, bits, error, path + "[1]") && parseFloat(value[2], out.b, bits, error, path + "[2]") && parseFloat(value[3], out.a, bits, error, path + "[3]");
}

bool parseGeometryValue(const json& value, VectorPathGeometry& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (!value.is_object()) { error = failMessage(path, "expected geometry wrapper"); return false; }
    const int variant = value.value("variant", 0);
    const auto& inner = value.value("value", json::object());
    if (variant != 0) { out = VectorPathGeometry{}; return true; }
    if (!inner.is_object()) { error = failMessage(path, "invalid geometry value"); return false; }
    for (std::size_t i = 0; i < inner.value("segments", json::array()).size(); ++i) {
        const auto& segment = inner["segments"][i];
        EraseCubicSegment cubic;
        const auto& p0 = segment["p0"]; const auto& p1 = segment["p1"];
        if (!parseVec(p0["position"], cubic.p0.position, bits, error, path + ".segments.p0.position") || !parseDouble(p0.value("radius", 0.0), cubic.p0.radius, bits, error, path + ".segments.p0.radius") || !parseVec(p1["position"], cubic.p1.position, bits, error, path + ".segments.p1.position") || !parseDouble(p1.value("radius", 0.0), cubic.p1.radius, bits, error, path + ".segments.p1.radius") || !parseVec(segment.value("control1", json::array({0.0, 0.0})), cubic.control1, bits, error, path + ".segments.control1") || !parseVec(segment.value("control2", json::array({0.0, 0.0})), cubic.control2, bits, error, path + ".segments.control2")) return false;
        out.commands.push_back(MoveTo{cubic.p0.position});
        out.commands.push_back(CubicTo{cubic.control1, cubic.control2, cubic.p1.position});
    }
    return true;
}

bool parseEraseGeometry(const json& value, EraseMaskGeometry& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    const int variant = value.value("variant", 0); const auto& inner = value.value("value", json::object());
    if (variant == 0) { SweptCircleMask mask; if (!inner.is_object()) { error = failMessage(path, "invalid mask value"); return false; } for (std::size_t i = 0; i < inner.value("segments", json::array()).size(); ++i) { const auto& segment = inner["segments"][i]; EraseCubicSegment s; if (!parseVec(segment["p0"]["position"], s.p0.position, bits, error, path) || !parseDouble(segment["p0"].value("radius", 0.0), s.p0.radius, bits, error, path) || !parseVec(segment["p1"]["position"], s.p1.position, bits, error, path) || !parseDouble(segment["p1"].value("radius", 0.0), s.p1.radius, bits, error, path) || !parseVec(segment.value("control1", json::array({0.0, 0.0})), s.control1, bits, error, path) || !parseVec(segment.value("control2", json::array({0.0, 0.0})), s.control2, bits, error, path)) return false; mask.segments.push_back(s); } out = std::move(mask); return true; }
    FilledPathMask mask; if (!parseGeometryValue(value, mask.path, bits, error, path)) return false; out = std::move(mask); return true;
}

bool parseTextStyle(const json& value, TextStyle& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (!value.is_object()) { error = failMessage(path, "expected text style"); return false; }
    if (value.contains("font_resource_id")) { ObjectId id; if (!parseId(value["font_resource_id"], id, error, path + ".font_resource_id")) return false; out.font_resource_id = ResourceId{id}; }
    if (!parseDouble(value.value("font_size", 0.0), out.font_size, bits, error, path + ".font_size") || !parseU32(value.value("weight", 0), out.weight, error, path + ".weight")) return false;
    out.italic = value.value("italic", false); out.underline = value.value("underline", false);
    if (value.contains("color")) return parseColor(value["color"], out.color, bits, error, path + ".color");
    return true;
}

bool parseParagraphStyle(const json& value, ParagraphStyle& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (!value.is_object()) { error = failMessage(path, "expected paragraph style"); return false; }
    std::uint32_t alignment = 0; if (!parseU32(value.value("alignment", 0), alignment, error, path + ".alignment")) return false; out.alignment = static_cast<ParagraphAlignment>(alignment);
    return parseDouble(value.value("line_height", 0.0), out.line_height, bits, error, path + ".line_height") && parseDouble(value.value("spacing_before", 0.0), out.spacing_before, bits, error, path + ".spacing_before") && parseDouble(value.value("spacing_after", 0.0), out.spacing_after, bits, error, path + ".spacing_after");
}

bool parseContent(const json& value, ObjectContent& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (!value.is_object()) { error = failMessage(path, "expected content wrapper"); return false; }
    const int variant = value.value("variant", 0); const auto& inner = value.value("value", json::object());
    switch (variant) {
        case 0: { ShapeContent x; x.shape_kind = inner.value("shape_kind", 1U); if (!parseDouble(inner.value("width", 0.0), x.width, bits, error, path) || !parseDouble(inner.value("height", 0.0), x.height, bits, error, path)) return false; out = x; return true; }
        case 1: { ImageContent x; ObjectId id; if (inner.contains("resource_id") && !parseId(inner["resource_id"], id, error, path)) return false; x.resource_id = ResourceId{id}; if (!parseDouble(inner.value("intrinsic_width", 0.0), x.intrinsic_width, bits, error, path) || !parseDouble(inner.value("intrinsic_height", 0.0), x.intrinsic_height, bits, error, path) || !parseDouble(inner.value("width", 0.0), x.width, bits, error, path) || !parseDouble(inner.value("height", 0.0), x.height, bits, error, path)) return false; x.content_mode = static_cast<ImageContentMode>(inner.value("content_mode", 1)); if (inner.contains("source_rect")) { NormalizedRect r; const auto& rv = inner["source_rect"]; if (!parseDouble(rv.value("x", 0.0), r.x, bits, error, path) || !parseDouble(rv.value("y", 0.0), r.y, bits, error, path) || !parseDouble(rv.value("width", 0.0), r.width, bits, error, path) || !parseDouble(rv.value("height", 0.0), r.height, bits, error, path)) return false; x.source_rect = r; } out = x; return true; }
        case 2: { VectorPathContent x; if (!parseGeometryValue(inner.value("geometry", json::object()), x.geometry, bits, error, path)) return false; out = x; return true; }
        case 3: { RichTextContent x; const auto& doc = inner.value("document", json::object()); for (const auto& paragraph : doc.value("paragraphs", json::array())) { Paragraph p; if (!parseId(paragraph.value("id", "00000000000000000000000000000001"), p.id, error, path)) return false; if (!parseParagraphStyle(paragraph.value("style", json::object()), p.style, bits, error, path)) return false; for (const auto& run : paragraph.value("runs", json::array())) { TextRun r; r.text = run.value("text", ""); if (!parseTextStyle(run.value("style", json::object()), r.style, bits, error, path)) return false; p.runs.push_back(std::move(r)); } x.document.paragraphs.push_back(std::move(p)); } out = x; return true; }
        case 4: case 5: { StrokeRecord stroke; stroke.deterministic_seed = inner.value("stroke", json::object()).value("deterministic_seed", 1ULL); const auto& s = inner.value("stroke", json::object()); stroke.brush.brush_family_id = s.value("brush_family_id", 1U); stroke.brush.brush_version = s.value("brush_version", 1U); if (!parseDouble(s.value("nominal_size", 1.0), stroke.brush.nominal_size, bits, error, path) || !parseFloat(s.value("opacity", 1.0), stroke.brush.opacity, bits, error, path)) return false; VectorStrokeData data; for (const auto& sample : s.value("data", json::array())) { StrokeSample x; if (!parseVec(sample.value("position", json::array({0.0, 0.0})), x.position, bits, error, path) || !parseFloat(sample.value("pressure", 1.0), x.pressure, bits, error, path) || !parseVec(sample.value("tilt", json::array({0.0, 0.0})), x.tilt, bits, error, path)) return false; data.samples.push_back(x); } stroke.data = data; if (variant == 4) out = VectorStrokeContent{stroke}; else out = DabStrokeContent{stroke}; return true; }
        case 6: { ConnectorContent x; if (!inner.is_object()) { error = failMessage(path, "invalid connector"); return false; } auto parseEndpoint = [&](const json& e, ConnectorEndpoint& endpoint) -> bool { const int ev = e.value("variant", 0); const auto& v = e.value("value", json::object()); if (ev == 0) { FreePointEndpoint p; return parseVec(v.value("point", json::array({0.0, 0.0})), p.point, bits, error, path) ? (endpoint.value = p, true) : false; } AttachedEndpoint a; if (!parseId(v.value("target_object_id", "00000000000000000000000000000001"), a.target_object_id, error, path)) return false; const auto& av = v.value("anchor", json::object()); const int avar = av.value("variant", 0); if (avar == 0) { StablePortAnchor port; if (!parseU32(av.value("value", json::object()).value("port_id", 0), port.port_id, error, path)) return false; a.anchor = port; } else a.anchor = AutoPerimeterAnchor{}; endpoint.value = a; return true; }; if (!parseEndpoint(inner.value("start", json::object()), x.start) || !parseEndpoint(inner.value("end", json::object()), x.end)) return false; x.routing = static_cast<ConnectorRouting>(inner.value("routing", 1)); out = x; return true; }
        case 7: { StickyContent x; if (!parseDouble(inner.value("width", 0.0), x.width, bits, error, path) || !parseDouble(inner.value("height", 0.0), x.height, bits, error, path)) return false; out = x; return true; }
        case 8: out = GroupContent{}; return true;
        default: out = ShapeContent{}; return true;
    }
}

bool parseObject(const json& value, ObjectRecord& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    if (!value.is_object()) { error = failMessage(path, "expected object record"); return false; }
    if (!parseId(value.value("id", "00000000000000000000000000000001"), out.id, error, path + ".id")) return false;
    std::uint32_t kind = 1; if (!parseU32(value.value("kind", 1), kind, error, path + ".kind")) return false; out.kind = static_cast<ObjectKind>(kind); if (!parseU32(value.value("kind_version", 1), out.kind_version, error, path + ".kind_version")) return false;
    if (!parsePlacement(value.value("placement", json::object()), out.placement, bits, error, path + ".placement")) return false;
    const auto& transform = value.value("transform", json::array({1.0, 0.0, 0.0, 1.0, 0.0, 0.0})); if (!transform.is_array() || transform.size() != 6U) { error = failMessage(path + ".transform", "expected six values"); return false; }
    if (!parseDouble(transform[0], out.transform.a, bits, error, path) || !parseDouble(transform[1], out.transform.b, bits, error, path) || !parseDouble(transform[2], out.transform.c, bits, error, path) || !parseDouble(transform[3], out.transform.d, bits, error, path) || !parseDouble(transform[4], out.transform.tx, bits, error, path) || !parseDouble(transform[5], out.transform.ty, bits, error, path)) return false;
    if (!parseContent(value.value("content", json::object()), out.content, bits, error, path + ".content")) return false;
    for (const auto& mask : value.value("erase_masks", json::array())) { EraseMaskRecord m; if (!parseId(mask.value("id", "00000000000000000000000000000001"), m.id, error, path)) return false; if (!parseEraseGeometry(mask.value("geometry", json::object()), m.geometry, bits, error, path)) return false; out.erase_masks.push_back(std::move(m)); }
    return true;
}

bool parsePropertyValue(const json& value, PropertyValue& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    const int variant = value.value("variant", 0); const auto& inner = value.value("value", json());
    switch (variant) {
        case 0: out = inner.is_boolean() ? PropertyValue(inner.get<bool>()) : PropertyValue(true); return true;
        case 1: if (inner.is_number()) { float f; if (!parseFloat(inner, f, bits, error, path)) return false; out = f; } else out = true; return true;
        case 2: { ColorValue c; if (inner.is_array() && parseColor(inner, c, bits, error, path)) out = c; else out = ColorValue{}; return true; }
        case 3: out = FillStyleValue{NoFill{}}; return true;
        case 4: out = StrokeStyleValue{NoStroke{}}; return true;
        case 5: out = BlendModeValue::kNormal; return true;
        case 6: out = ConnectorDecorationValue::kNone; return true;
        default: out = true; return true;
    }
}

bool parseOperation(const json& value, Operation& out, std::vector<std::uint64_t>& bits, std::string& error, const std::string& path) {
    ObjectId operationId; ObjectId documentId;
    if (!parseId(value.value("id", "00000000000000000000000000000001"), operationId, error, path + ".id")) return false;
    if (!parseId(value.value("document_id", "00000000000000000000000000000001"), documentId, error, path + ".document_id")) return false;
    out.id = OperationId(operationId); out.document_id = DocumentId(documentId);
    if (!parseU32(value.value("schema_version", 1), out.schema_version, error, path) || !parseU32(value.value("payload_version", 1), out.payload_version, error, path)) return false;
    const auto& wrapper = value.value("payload", json::object()); const int variant = wrapper.value("variant", 0); const auto& p = wrapper.value("value", json::object());
    const std::string base = path + ".payload";
    auto objectList = [&](const char* key, std::vector<ObjectRecord>& objects) { for (const auto& item : p.value(key, json::array())) { ObjectRecord record; if (!parseObject(item, record, bits, error, base)) return false; objects.push_back(std::move(record)); } return true; };
    switch (variant) {
        case 0: { InsertObjectsOp x; if (!objectList("objects", x.objects)) return false; out.payload = std::move(x); break; }
        case 1: { DeleteObjectsOp x; for (const auto& id : p.value("object_ids", json::array())) { ObjectId v; if (!parseId(id, v, error, base)) return false; x.object_ids.push_back(v); } out.payload = std::move(x); break; }
        case 2: { RestoreObjectsOp x; if (!objectList("objects", x.objects)) return false; out.payload = std::move(x); break; }
        case 3: { SetPlacementsOp x; for (const auto& item : p.value("items", json::array())) { PlacementItem i; if (!parseId(item.value("object_id", "00000000000000000000000000000001"), i.object_id, error, base) || !parsePlacement(item.value("placement", json::object()), i.placement, bits, error, base)) return false; x.items.push_back(i); } out.payload = std::move(x); break; }
        case 4: { SetTransformsOp x; for (const auto& item : p.value("items", json::array())) { TransformItem i; if (!parseId(item.value("object_id", "00000000000000000000000000000001"), i.object_id, error, base)) return false; const auto& t=item.value("transform",json::array()); if(t.size()!=6){error=failMessage(base,"transform size");return false;} if(!parseDouble(t[0],i.transform.a,bits,error,base)||!parseDouble(t[1],i.transform.b,bits,error,base)||!parseDouble(t[2],i.transform.c,bits,error,base)||!parseDouble(t[3],i.transform.d,bits,error,base)||!parseDouble(t[4],i.transform.tx,bits,error,base)||!parseDouble(t[5],i.transform.ty,bits,error,base))return false; x.items.push_back(i);} out.payload=std::move(x); break; }
        case 5: { PatchPropertiesOp x; for (const auto& item : p.value("patches", json::array())) { PropertyPatch i; if(!parseId(item.value("object_id","00000000000000000000000000000001"),i.object_id,error,base)||!parseU32(item.value("field_id",0),i.field_id,error,base))return false; const auto action=item.value("action","invalid"); i.action=action=="set"?PropertyPatchAction::kSet:action=="clear"?PropertyPatchAction::kClear:PropertyPatchAction::kInvalid; if(item.contains("value")){PropertyValue v;if(!parsePropertyValue(item["value"],v,bits,error,base))return false;i.value=v;} x.patches.push_back(std::move(i));} out.payload=std::move(x); break; }
        case 6: { SetObjectSizeOp x; for (const auto& item:p.value("items",json::array())){ObjectSizeItem i; json width=item.contains("width")?item["width"]:json(0.0); json height=item.contains("height")?item["height"]:json(0.0);if(!parseId(item.value("object_id","00000000000000000000000000000001"),i.object_id,error,base)||!parseDouble(width,i.width,bits,error,base)||!parseDouble(height,i.height,bits,error,base))return false;x.items.push_back(i);}out.payload=std::move(x);break; }
        case 7: { SetVectorPathGeometryOp x;if(!parseId(p.value("object_id","00000000000000000000000000000001"),x.object_id,error,base)||!parseGeometryValue(p.value("geometry",json::object()),x.geometry,bits,error,base))return false;out.payload=x;break; }
        case 8: { SetImageContentOp x;if(!parseId(p.value("object_id","00000000000000000000000000000001"),x.object_id,error,base))return false;ObjectContent c;if(!parseContent(json{{"variant",1},{"value",p.value("content",json::object())}},c,bits,error,base))return false;x.content=std::get<ImageContent>(c);out.payload=x;break; }
        case 9: { AddStrokeOp x;if(!parseObject(p.value("object",json::object()),x.object,bits,error,base))return false;out.payload=x;break; }
        case 10: { SplitStrokesOp x;for(const auto&s:p.value("splits",json::array())){StrokeSplit z;if(!parseId(s.value("source_stroke_id","00000000000000000000000000000001"),z.source_stroke_id,error,base))return false;for(const auto& n:s.value("replacements",json::array())){ObjectRecord r;if(!parseObject(n,r,bits,error,base))return false;z.replacements.push_back(std::move(r));}x.splits.push_back(std::move(z));}out.payload=x;break; }
        case 11: { AddEraseMasksOp x;for(const auto&i:p.value("items",json::array())){EraseMaskAddItem z;if(!parseId(i.value("object_id","00000000000000000000000000000001"),z.object_id,error,base))return false;for(const auto&m:i.value("masks",json::array())){EraseMaskRecord r;if(!parseId(m.value("id","00000000000000000000000000000001"),r.id,error,base)||!parseEraseGeometry(m.value("geometry",json::object()),r.geometry,bits,error,base))return false;z.masks.push_back(std::move(r));}x.items.push_back(std::move(z));}out.payload=x;break; }
        case 12: { RemoveEraseMasksOp x;for(const auto&i:p.value("items",json::array())){EraseMaskRemoveItem z;if(!parseId(i.value("object_id","00000000000000000000000000000001"),z.object_id,error,base))return false;for(const auto&id:i.value("mask_ids",json::array())){ObjectId v;if(!parseId(id,v,error,base))return false;z.mask_ids.push_back(v);}x.items.push_back(std::move(z));}out.payload=x;break; }
        case 13: { EditRichTextOp x;if(!parseId(p.value("object_id","00000000000000000000000000000001"),x.object_id,error,base))return false;const auto& d=p.value("delta",json::object());x.delta.delta_version=d.value("delta_version",1U);for(const auto&s:d.value("steps",json::array())){const auto kind=s.value("kind","InsertText");if(kind=="InsertText"){InsertTextStep z;if(!parseId(s.value("paragraph_id","00000000000000000000000000000001"),z.paragraph_id,error,base)||!parseU32(s.value("scalar_offset",0),z.scalar_offset,error,base)||!parseTextStyle(s.value("style",json::object()),z.style,bits,error,base))return false;z.text=s.value("text","");x.delta.steps.push_back(std::move(z));}else{x.delta.steps.push_back(DeleteTextStep{});}}out.payload=x;break; }
        case 14: { SetConnectorContentOp x;if(!parseId(p.value("object_id","00000000000000000000000000000001"),x.object_id,error,base))return false;ObjectContent c;if(!parseContent(json{{"variant",6},{"value",p.value("content",json::object())}},c,bits,error,base))return false;x.content=std::get<ConnectorContent>(c);out.payload=x;break; }
        default: out.payload = InsertObjectsOp{}; break;
    }
    return true;
}

} // namespace

StringDecodeResult decodeStringScalar(std::string_view value) { return StringDecodeResult{true, std::string(value), {}}; }

DecodeResult decodeFixtureJson(std::string_view jsonText) {
    DecodeResult result;
    try {
        const auto root = json::parse(jsonText.begin(), jsonText.end());
        if (root.value("format", "") != "axiom-g1-04-c-input-v1") { result.error = "unsupported fixture format"; return result; }
        DecodedFixture fixture; fixture.caseId = root.value("caseId", ""); fixture.operationFamily = root.value("operationFamily", "");
        for (const auto& object : root.value("initialState", json::object()).value("objects", json::array())) { ObjectRecord r; if (!parseObject(object, r, fixture.f64Bits, result.error, "initialState.objects")) return result; fixture.initialObjects.push_back(std::move(r)); }
        if (!parseOperation(root.value("operation", json::object()), fixture.operation, fixture.f64Bits, result.error, "operation")) return result;
        for (const auto& prior : root.value("initialState", json::object()).value("priorOperations", json::array())) { Operation op; json wrapped = prior; wrapped["id"] = prior.value("operation_id", "00000000000000000000000000000001"); if (!parseOperation(wrapped, op, fixture.f64Bits, result.error, "initialState.priorOperations")) return result; fixture.priorOperations.push_back(std::move(op)); }
        result.ok = true; result.fixture = std::move(fixture); return result;
    } catch (const std::exception& e) { result.error = e.what(); return result; }
}

DecodeResult decodeFixtureFile(const std::string& path) {
    std::ifstream input(path); if (!input) return DecodeResult{false, "unable to open fixture: " + path};
    std::ostringstream contents; contents << input.rdbuf(); return decodeFixtureJson(contents.str());
}

} // namespace canvas::verification::g1_04_c
