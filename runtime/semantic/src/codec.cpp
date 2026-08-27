#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/object_content.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>

#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/brush_stroke.pb.h"
#include "auditoryworks/axiom/v1/common.pb.h"
#include "auditoryworks/axiom/v1/geometry.pb.h"
#include "auditoryworks/axiom/v1/operation.pb.h"
#include "auditoryworks/axiom/v1/object.pb.h"
#include "auditoryworks/axiom/v1/paint.pb.h"
#include "auditoryworks/axiom/v1/property.pb.h"
#include "auditoryworks/axiom/v1/rich_text.pb.h"
#include "auditoryworks/axiom/v1/snapshot.pb.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#endif

namespace {
constexpr std::uint8_t kMagic0 = 0x41U;
constexpr std::uint8_t kMagic1 = 0x58U;
constexpr std::uint8_t kVersion = 1U;
constexpr std::size_t kHeaderBytes = 8U;
constexpr std::size_t kMaxFieldBytes = 1024U * 1024U;

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
}
void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32U; shift += 8U) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) | static_cast<std::uint16_t>(bytes[offset + 1U] << 8U);
}
std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    std::uint32_t result = 0;
    for (unsigned shift = 0; shift < 32U; shift += 8U) result |= static_cast<std::uint32_t>(bytes[offset + shift / 8U]) << shift;
    return result;
}

#if defined(CANVAS_SEMANTIC_PROTOBUF)
struct WireField final {
    std::uint32_t number = 0;
    std::uint8_t type = 0;
    std::vector<std::uint8_t> value;
};

bool readVarint(const std::vector<std::uint8_t>& bytes, std::size_t& offset, std::uint64_t& value) {
    value = 0U;
    for (unsigned shift = 0; shift < 64U; shift += 7U) {
        if (offset >= bytes.size()) return false;
        const auto byte = bytes[offset++];
        value |= static_cast<std::uint64_t>(byte & 0x7fU) << shift;
        if ((byte & 0x80U) == 0U) return true;
    }
    return false;
}

bool scanWire(const std::vector<std::uint8_t>& bytes, std::vector<WireField>& fields) {
    std::size_t offset = 0U;
    while (offset < bytes.size()) {
        std::uint64_t key = 0U;
        if (!readVarint(bytes, offset, key) || key == 0U) return false;
        const auto number = static_cast<std::uint32_t>(key >> 3U);
        const auto type = static_cast<std::uint8_t>(key & 0x07U);
        if (number == 0U) return false;
        switch (type) {
            case 0U: {
                const auto begin = offset;
                std::uint64_t value = 0U;
                if (!readVarint(bytes, offset, value)) return false;
                fields.push_back({number, type, {bytes.begin() + static_cast<std::ptrdiff_t>(begin), bytes.begin() + static_cast<std::ptrdiff_t>(offset)}});
                break;
            }
            case 1U: {
                const auto begin = offset;
                if (bytes.size() - offset < 8U) return false;
                offset += 8U;
                fields.push_back({number, type, {bytes.begin() + static_cast<std::ptrdiff_t>(begin), bytes.begin() + static_cast<std::ptrdiff_t>(offset)}});
                break;
            }
            case 2U: {
                std::uint64_t length = 0U;
                if (!readVarint(bytes, offset, length) || length > bytes.size() - offset) return false;
                const auto begin = offset;
                offset += static_cast<std::size_t>(length);
                fields.push_back({number, type, {bytes.begin() + static_cast<std::ptrdiff_t>(begin), bytes.begin() + static_cast<std::ptrdiff_t>(offset)}});
                break;
            }
            case 5U: {
                const auto begin = offset;
                if (bytes.size() - offset < 4U) return false;
                offset += 4U;
                fields.push_back({number, type, {bytes.begin() + static_cast<std::ptrdiff_t>(begin), bytes.begin() + static_cast<std::ptrdiff_t>(offset)}});
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

bool allowsField(const std::string& root_type, std::uint32_t field) {
    if (root_type == "Id128" || root_type == "OrderKey") return field == 1U;
    if (root_type == "Vec2") return field == 1U || field == 2U;
    if (root_type == "Transform2D") return field >= 1U && field <= 6U;
    if (root_type == "PropertyValue") return field >= 1U && field <= 7U;
    if (root_type == "ColorValue") return field >= 1U && field <= 4U;
    if (root_type == "Placement") return field == 1U || field == 2U;
    if (root_type == "DashPattern") return field == 1U || field == 2U;
    if (root_type == "DocumentSnapshot") return field >= 1U && field <= 3U;
    if (root_type == "ParagraphStyle") return field >= 1U && field <= 4U;
    if (root_type == "RichTextDocument") return field == 1U;
    if (root_type == "RichTextDelta") return field == 1U || field == 2U;
    if (root_type == "RichTextStep") return field >= 1U && field <= 6U;
    if (root_type == "PressureMapping") return field >= 1U && field <= 3U;
    if (root_type == "BrushDescriptor") return field >= 1U && field <= 11U;
    if (root_type == "StrokeRecord") return field >= 1U && field <= 4U;
    return false;
}

bool hasExpectedWireType(const std::string& root_type, std::uint32_t field, std::uint8_t type) {
    if (root_type == "Id128" || root_type == "OrderKey") return type == 2U;
    if (root_type == "Vec2") return type == 1U;
    if (root_type == "Transform2D") return type == 1U;
    if (root_type == "PropertyValue") return (field == 2U && type == 5U) || (field != 2U && type == 0U);
    if (root_type == "ColorValue") return type == 5U;
    if (root_type == "Placement") return type == 2U;
    if (root_type == "DashPattern") return (field == 1U && type == 2U) || (field == 2U && type == 1U);
    if (root_type == "DocumentSnapshot") return (field == 1U && type == 2U) || (field == 2U && type == 0U) || (field == 3U && type == 2U);
    if (root_type == "ParagraphStyle") return (field == 1U && type == 0U) || (field >= 2U && type == 1U);
    if (root_type == "RichTextDocument") return field == 1U && type == 2U;
    if (root_type == "RichTextDelta") return (field == 1U && type == 0U) || (field == 2U && type == 2U);
    if (root_type == "RichTextStep") return type == 2U;
    if (root_type == "PressureMapping") return (field == 1U && type == 0U) || (field >= 2U && type == 2U);
    if (root_type == "BrushDescriptor") {
        return ((field == 1U || field == 2U || field == 11U) && type == 0U) ||
            (field == 4U && type == 1U) || (field == 5U && type == 5U) ||
            ((field == 3U || (field >= 6U && field <= 10U)) && type == 2U);
    }
    if (root_type == "StrokeRecord") return (field == 2U && type == 1U) || (field != 2U && type == 2U);
    return false;
}

bool validRichTextStepPayload(std::uint32_t branch, const std::vector<std::uint8_t>& bytes) {
    std::vector<WireField> fields;
    if (!scanWire(bytes, fields) || branch < 1U || branch > 6U) return false;
    for (const auto& field : fields) {
        const bool is_id =
            (branch == 1U && (field.number == 1U || field.number == 4U)) ||
            (branch == 2U && field.number == 1U) ||
            (branch == 3U && (field.number == 1U || field.number == 3U)) ||
            (branch == 4U && (field.number == 1U || field.number == 2U)) ||
            (branch == 5U && (field.number == 1U || field.number == 4U)) ||
            (branch == 6U && (field.number == 1U || field.number == 2U));
        const bool is_scalar =
            (branch == 1U && field.number == 2U) ||
            (branch == 2U && (field.number == 2U || field.number == 3U)) ||
            (branch == 3U && field.number == 2U) ||
            (branch == 5U && (field.number == 2U || field.number == 3U));
        const bool is_text = branch == 1U && field.number == 3U;
        if ((!is_id && !is_scalar && !is_text) ||
            (is_id && field.type != 2U) || (is_scalar && field.type != 0U) || (is_text && field.type != 2U)) {
            return false;
        }
    }
    return true;
}

std::string preflightCategory(const std::string& root_type, const std::vector<std::uint8_t>& bytes, bool strict_canonical) {
    std::vector<WireField> fields;
    if (!scanWire(bytes, fields)) return "MALFORMED_WIRE";
    std::vector<std::uint32_t> seen;
    unsigned property_oneof_members = 0U;
    for (const auto& field : fields) {
        if (!allowsField(root_type, field.number)) return "UNKNOWN_WIRE_FIELD";
        if (!hasExpectedWireType(root_type, field.number, field.type)) return "UNKNOWN_WIRE_FIELD";
        if (root_type == "DashPattern" && field.number == 1U && field.type != 2U && strict_canonical) {
            return "NON_CANONICAL_PACKED_ENCODING";
        }
        if (root_type == "PropertyValue" && field.number >= 1U && field.number <= 7U) {
            ++property_oneof_members;
        }
        const bool repeated_field =
            (root_type == "DashPattern" && field.number == 1U && field.type == 1U) ||
            (root_type == "RichTextDelta" && field.number == 2U && field.type == 2U);
        if (!repeated_field && std::find(seen.begin(), seen.end(), field.number) != seen.end()) {
            return "DUPLICATE_SINGULAR_FIELD";
        }
        seen.push_back(field.number);
    }
    if (root_type == "PropertyValue" && property_oneof_members > 1U) return "MULTIPLE_ONEOF_MEMBERS";
    if (root_type == "RichTextDelta") {
        for (const auto& field : fields) {
            if (field.number != 2U) continue;
            const auto nested = preflightCategory("RichTextStep", field.value, strict_canonical);
            if (!nested.empty()) return nested;
        }
    }
    if (root_type == "RichTextStep") {
        for (const auto& field : fields) {
            if (!validRichTextStepPayload(field.number, field.value)) return "UNKNOWN_WIRE_FIELD";
        }
    }
    return {};
}

bool isFiniteValue(double value) { return std::isfinite(value); }

template <typename StringLike>
bool validId(const StringLike& value) { return value.size() == 16U; }

template <typename StringLike>
bool validOrderKey(const StringLike& value) {
    return !value.empty() && value.size() <= 32U && value[value.size() - 1U] != '\0';
}

void appendVarint(std::vector<std::uint8_t>& output, std::uint64_t value) {
    while (value > 0x7fU) {
        output.push_back(static_cast<std::uint8_t>((value & 0x7fU) | 0x80U));
        value >>= 7U;
    }
    output.push_back(static_cast<std::uint8_t>(value));
}

template <typename StringLike>
void appendBytes(std::vector<std::uint8_t>& output, std::uint32_t field, const StringLike& value) {
    appendVarint(output, static_cast<std::uint64_t>((field << 3U) | 2U));
    appendVarint(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

void appendFixed64(std::vector<std::uint8_t>& output, std::uint32_t field, double value) {
    output.push_back(static_cast<std::uint8_t>((field << 3U) | 1U));
    if (value == 0.0) value = 0.0;
    const auto bits = std::bit_cast<std::uint64_t>(value);
    for (unsigned shift = 0; shift < 64U; shift += 8U) {
        output.push_back(static_cast<std::uint8_t>(bits >> shift));
    }
}

void appendFixed32(std::vector<std::uint8_t>& output, std::uint32_t field, float value) {
    if (value == 0.0F) value = 0.0F;
    output.push_back(static_cast<std::uint8_t>((field << 3U) | 5U));
    const auto bits = std::bit_cast<std::uint32_t>(value);
    for (unsigned shift = 0; shift < 32U; shift += 8U) {
        output.push_back(static_cast<std::uint8_t>(bits >> shift));
    }
}

void appendMessage(std::vector<std::uint8_t>& output, std::uint32_t field, const std::vector<std::uint8_t>& value) {
    appendVarint(output, static_cast<std::uint64_t>((field << 3U) | 2U));
    appendVarint(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

std::string bytesHex(const std::array<std::uint8_t, 16>& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2U);
    for (const auto byte : bytes) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

template <typename Number>
std::string jsonNumber(Number value) {
    if (value == static_cast<Number>(0)) value = static_cast<Number>(0);
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<Number>::max_digits10) << value;
    return output.str();
}

std::string jsonString(const std::string& value) {
    std::string result{"\""};
    for (const unsigned char byte : value) {
        switch (byte) {
            case '\\': result += "\\\\"; break;
            case '\"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (byte < 0x20U) {
                    constexpr char digits[] = "0123456789abcdef";
                    result += "\\u00";
                    result.push_back(digits[byte >> 4U]);
                    result.push_back(digits[byte & 0x0fU]);
                } else {
                    result.push_back(static_cast<char>(byte));
                }
        }
    }
    result += "\"";
    return result;
}

bool mapId(const auditoryworks::axiom::v1::Id128& source, canvas::semantic::ObjectId& destination) {
    if (!source.has_value() || !validId(source.value())) return false;
    for (std::size_t index = 0; index < destination.bytes.size(); ++index) {
        destination.bytes[index] = static_cast<std::uint8_t>(source.value()[index]);
    }
    return true;
}

bool mapVec(const auditoryworks::axiom::v1::Vec2& source, canvas::semantic::Vec2& destination) {
    if (!source.has_x() || !source.has_y()) return false;
    destination = {source.x(), source.y()};
    return true;
}

bool mapTextStyle(const auditoryworks::axiom::v1::TextStyle& source, canvas::semantic::TextStyle& destination) {
    if (source.has_font_resource_id()) {
        canvas::semantic::ObjectId id;
        if (!mapId(source.font_resource_id(), id)) return false;
        destination.font_resource_id = canvas::semantic::ResourceId{id};
    }
    destination.font_size = source.font_size();
    destination.weight = source.weight();
    destination.italic = source.italic();
    destination.underline = source.underline();
    if (source.has_color()) destination.color = {source.color().r(), source.color().g(), source.color().b(), source.color().a()};
    return true;
}

bool mapParagraphStyle(const auditoryworks::axiom::v1::ParagraphStyle& source, canvas::semantic::ParagraphStyle& destination) {
    destination.alignment = static_cast<canvas::semantic::ParagraphAlignment>(source.alignment());
    destination.line_height = source.line_height();
    destination.spacing_before = source.spacing_before();
    destination.spacing_after = source.spacing_after();
    return true;
}

bool mapTextRun(const auditoryworks::axiom::v1::TextRun& source, canvas::semantic::TextRun& destination) {
    destination.text.assign(source.text().data(), source.text().size());
    return mapTextStyle(source.style(), destination.style);
}

bool mapParagraph(const auditoryworks::axiom::v1::Paragraph& source, canvas::semantic::Paragraph& destination) {
    if (!mapId(source.paragraph_id(), destination.id) || !mapParagraphStyle(source.style(), destination.style)) return false;
    destination.runs.clear();
    destination.runs.reserve(static_cast<std::size_t>(source.runs_size()));
    for (const auto& run : source.runs()) {
        canvas::semantic::TextRun mapped;
        if (!mapTextRun(run, mapped)) return false;
        destination.runs.push_back(std::move(mapped));
    }
    return true;
}

bool mapRichTextDocument(const auditoryworks::axiom::v1::RichTextDocument& source, canvas::semantic::RichTextDocument& destination) {
    destination.paragraphs.clear();
    destination.paragraphs.reserve(static_cast<std::size_t>(source.paragraphs_size()));
    for (const auto& paragraph : source.paragraphs()) {
        canvas::semantic::Paragraph mapped;
        if (!mapParagraph(paragraph, mapped)) return false;
        destination.paragraphs.push_back(std::move(mapped));
    }
    return true;
}

bool mapRichTextStep(const auditoryworks::axiom::v1::RichTextStep& source, canvas::semantic::RichTextStep& destination) {
    using namespace auditoryworks::axiom::v1;
    canvas::semantic::ObjectId first;
    canvas::semantic::ObjectId second;
    canvas::semantic::TextStyle text_style;
    canvas::semantic::ParagraphStyle paragraph_style;
    if (source.has_insert_text()) {
        const auto& item = source.insert_text();
        if (!mapId(item.paragraph_id(), first) || !mapTextStyle(item.style(), text_style)) return false;
        destination = canvas::semantic::InsertTextStep{
            first, item.scalar_offset(), std::string(item.text().data(), item.text().size()), text_style};
    } else if (source.has_delete_text()) {
        const auto& item = source.delete_text();
        if (!mapId(item.paragraph_id(), first)) return false;
        destination = canvas::semantic::DeleteTextStep{first, item.start_scalar(), item.scalar_count()};
    } else if (source.has_split_paragraph()) {
        const auto& item = source.split_paragraph();
        if (!mapId(item.paragraph_id(), first) || !mapId(item.new_paragraph_id(), second)) return false;
        destination = canvas::semantic::SplitParagraphStep{first, item.scalar_offset(), second};
    } else if (source.has_merge_paragraph()) {
        const auto& item = source.merge_paragraph();
        if (!mapId(item.first_paragraph_id(), first) || !mapId(item.second_paragraph_id(), second)) return false;
        destination = canvas::semantic::MergeParagraphStep{first, second};
    } else if (source.has_set_inline_style()) {
        const auto& item = source.set_inline_style();
        if (!mapId(item.paragraph_id(), first) || !mapTextStyle(item.style(), text_style)) return false;
        destination = canvas::semantic::SetInlineStyleStep{first, item.start_scalar(), item.scalar_count(), text_style};
    } else if (source.has_set_paragraph_style()) {
        const auto& item = source.set_paragraph_style();
        if (!mapId(item.paragraph_id(), first) || !mapParagraphStyle(item.style(), paragraph_style)) return false;
        destination = canvas::semantic::SetParagraphStyleStep{first, paragraph_style};
    } else {
        return false;
    }
    return true;
}

bool mapRichTextDelta(const auditoryworks::axiom::v1::RichTextDelta& source, canvas::semantic::RichTextDelta& destination) {
    destination.delta_version = source.delta_version();
    destination.steps.clear();
    destination.steps.reserve(static_cast<std::size_t>(source.steps_size()));
    for (const auto& step : source.steps()) {
        canvas::semantic::RichTextStep mapped;
        if (!mapRichTextStep(step, mapped)) return false;
        destination.steps.push_back(std::move(mapped));
    }
    return true;
}

bool mapCurve(const auditoryworks::axiom::v1::PiecewiseLinearCurve01& source, canvas::semantic::PiecewiseLinearCurve01& destination) {
    destination.points.clear();
    destination.points.reserve(static_cast<std::size_t>(source.points_size()));
    for (const auto& point : source.points()) {
        if (!point.has_x() || !point.has_y()) return false;
        destination.points.push_back({point.x(), point.y()});
    }
    return true;
}

bool mapPressure(const auditoryworks::axiom::v1::PressureMapping& source, canvas::semantic::PressureMapping& destination) {
    destination.enabled = source.enabled();
    destination.size_curve.reset();
    destination.opacity_curve.reset();
    if (source.has_size_curve()) {
        canvas::semantic::PiecewiseLinearCurve01 curve;
        if (!mapCurve(source.size_curve(), curve)) return false;
        destination.size_curve = std::move(curve);
    }
    if (source.has_opacity_curve()) {
        canvas::semantic::PiecewiseLinearCurve01 curve;
        if (!mapCurve(source.opacity_curve(), curve)) return false;
        destination.opacity_curve = std::move(curve);
    }
    return true;
}

bool mapBrush(const auditoryworks::axiom::v1::BrushDescriptor& source, canvas::semantic::BrushDescriptor& destination) {
    destination.brush_family_id = source.brush_family_id();
    destination.brush_version = source.brush_version();
    destination.nominal_size = source.nominal_size();
    destination.opacity = source.opacity();
    if (source.has_color()) destination.color = {source.color().r(), source.color().g(), source.color().b(), source.color().a()};
    if (source.has_pressure() && !mapPressure(source.pressure(), destination.pressure)) return false;
    if (source.has_tilt()) destination.tilt = {source.tilt().enabled(), source.tilt().size_influence(), source.tilt().angle_influence()};
    if (source.has_smoothing()) destination.smoothing = {source.smoothing().amount()};
    if (source.has_spacing()) destination.spacing = {source.spacing().normalized_spacing()};
    destination.blend_mode = static_cast<canvas::semantic::BrushBlendMode>(source.blend_mode());
    destination.texture_resource_id.reset();
    if (source.has_texture_resource_id()) {
        canvas::semantic::ObjectId id;
        if (!mapId(source.texture_resource_id(), id)) return false;
        destination.texture_resource_id = canvas::semantic::ResourceId{id};
    }
    return true;
}

bool mapDabStroke(const auditoryworks::axiom::v1::DabStrokeData& source, canvas::semantic::DabStrokeData& destination) {
    destination.dabs.clear();
    destination.dabs.reserve(static_cast<std::size_t>(source.dabs_size()));
    for (const auto& dab : source.dabs()) {
        canvas::semantic::Vec2 center;
        if (!mapVec(dab.center(), center)) return false;
        destination.dabs.push_back({center, dab.size(), dab.rotation(), dab.opacity()});
    }
    return true;
}

bool mapStrokeRecord(const auditoryworks::axiom::v1::StrokeRecord& source, canvas::semantic::StrokeRecord& destination) {
    if (source.has_brush() && !mapBrush(source.brush(), destination.brush)) return false;
    destination.deterministic_seed = source.deterministic_seed();
    if (source.has_dab()) {
        canvas::semantic::DabStrokeData dabs;
        if (!mapDabStroke(source.dab(), dabs)) return false;
        destination.data = std::move(dabs);
    } else if (source.has_vector()) {
        canvas::semantic::VectorStrokeData vector;
        vector.samples.reserve(static_cast<std::size_t>(source.vector().samples_size()));
        for (const auto& sample : source.vector().samples()) {
            canvas::semantic::Vec2 position;
            if (!mapVec(sample.position(), position)) return false;
            vector.samples.push_back({position, sample.pressure(), {sample.tilt().x(), sample.tilt().y()}});
        }
        destination.data = std::move(vector);
    } else {
        return false;
    }
    return true;
}

std::string jsonVec(const canvas::semantic::Vec2& value) {
    return "{\"x\":" + jsonNumber(value.x) + ",\"y\":" + jsonNumber(value.y) + "}";
}

std::string jsonRichTextStep(const canvas::semantic::RichTextStep& step) {
    return std::visit([](const auto& item) -> std::string {
        using Item = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<Item, canvas::semantic::InsertTextStep>) {
            return "{\"kind\":\"InsertText\",\"paragraphId\":" + jsonString(bytesHex(item.paragraph_id.bytes)) +
                ",\"scalarOffset\":" + std::to_string(item.scalar_offset) + ",\"text\":" + jsonString(item.text) + "}";
        } else if constexpr (std::is_same_v<Item, canvas::semantic::DeleteTextStep>) {
            return "{\"kind\":\"DeleteText\",\"paragraphId\":" + jsonString(bytesHex(item.paragraph_id.bytes)) +
                ",\"startScalar\":" + std::to_string(item.start_scalar) + ",\"scalarCount\":" + std::to_string(item.scalar_count) + "}";
        } else if constexpr (std::is_same_v<Item, canvas::semantic::SplitParagraphStep>) {
            return "{\"kind\":\"SplitParagraph\",\"paragraphId\":" + jsonString(bytesHex(item.paragraph_id.bytes)) +
                ",\"scalarOffset\":" + std::to_string(item.scalar_offset) + ",\"newParagraphId\":" + jsonString(bytesHex(item.new_paragraph_id.bytes)) + "}";
        } else if constexpr (std::is_same_v<Item, canvas::semantic::MergeParagraphStep>) {
            return "{\"kind\":\"MergeParagraph\",\"firstParagraphId\":" + jsonString(bytesHex(item.first_paragraph_id.bytes)) +
                ",\"secondParagraphId\":" + jsonString(bytesHex(item.second_paragraph_id.bytes)) + "}";
        } else if constexpr (std::is_same_v<Item, canvas::semantic::SetInlineStyleStep>) {
            return "{\"kind\":\"SetInlineStyle\",\"paragraphId\":" + jsonString(bytesHex(item.paragraph_id.bytes)) +
                ",\"startScalar\":" + std::to_string(item.start_scalar) + ",\"scalarCount\":" + std::to_string(item.scalar_count) + "}";
        } else {
            return "{\"kind\":\"SetParagraphStyle\",\"paragraphId\":" + jsonString(bytesHex(item.paragraph_id.bytes)) + "}";
        }
    }, step);
}

std::string jsonRichTextDelta(const canvas::semantic::RichTextDelta& delta) {
    std::string result = "{\"deltaVersion\":" + std::to_string(delta.delta_version) + ",\"steps\":[";
    for (std::size_t index = 0; index < delta.steps.size(); ++index) {
        if (index != 0U) result += ',';
        result += jsonRichTextStep(delta.steps[index]);
    }
    return result + "]}";
}

std::string jsonParagraphStyle(const canvas::semantic::ParagraphStyle& style) {
    return "{\"alignment\":" + std::to_string(static_cast<unsigned>(style.alignment)) +
        ",\"lineHeight\":" + jsonNumber(style.line_height) +
        ",\"spacingBefore\":" + jsonNumber(style.spacing_before) +
        ",\"spacingAfter\":" + jsonNumber(style.spacing_after) + "}";
}

std::string jsonRichTextDocument(const canvas::semantic::RichTextDocument& document) {
    std::string result{"{\"paragraphs\":["};
    for (std::size_t index = 0; index < document.paragraphs.size(); ++index) {
        if (index != 0U) result += ',';
        const auto& paragraph = document.paragraphs[index];
        result += "{\"paragraphId\":" + jsonString(bytesHex(paragraph.id.bytes)) +
            ",\"style\":" + jsonParagraphStyle(paragraph.style) + ",\"runs\":[";
        for (std::size_t run_index = 0; run_index < paragraph.runs.size(); ++run_index) {
            if (run_index != 0U) result += ',';
            result += "{\"text\":" + jsonString(paragraph.runs[run_index].text) + "}";
        }
        result += "]}";
    }
    return result + "]}";
}

std::string jsonCurve(const canvas::semantic::PiecewiseLinearCurve01& curve) {
    std::string result = "[";
    for (std::size_t index = 0; index < curve.points.size(); ++index) {
        if (index != 0U) result += ',';
        result += "{\"x\":" + jsonNumber(curve.points[index].x) + ",\"y\":" + jsonNumber(curve.points[index].y) + "}";
    }
    return result + "]";
}

std::string jsonPressure(const canvas::semantic::PressureMapping& pressure) {
    std::string result = "{\"enabled\":" + std::string(pressure.enabled ? "true" : "false");
    if (pressure.size_curve.has_value()) result += ",\"sizeCurve\":" + jsonCurve(*pressure.size_curve);
    if (pressure.opacity_curve.has_value()) result += ",\"opacityCurve\":" + jsonCurve(*pressure.opacity_curve);
    return result + "}";
}

std::string jsonBrush(const canvas::semantic::BrushDescriptor& brush) {
    std::string result = "{\"brushFamilyId\":" + std::to_string(brush.brush_family_id) +
        ",\"brushVersion\":" + std::to_string(brush.brush_version) +
        ",\"pressure\":" + jsonPressure(brush.pressure) +
        ",\"spacing\":{\"normalizedSpacing\":" + jsonNumber(brush.spacing.normalized_spacing) + "}" +
        ",\"blendMode\":" + std::to_string(static_cast<unsigned>(brush.blend_mode));
    if (brush.texture_resource_id.has_value()) result += ",\"textureResourceId\":" + jsonString(bytesHex(brush.texture_resource_id->value.bytes));
    return result + "}";
}

std::string jsonStrokeRecord(const canvas::semantic::StrokeRecord& record) {
    std::string result = "{\"deterministicSeed\":" + jsonString(std::to_string(record.deterministic_seed)) + ",\"data\":";
    if (const auto* dabs = std::get_if<canvas::semantic::DabStrokeData>(&record.data)) {
        result += "{\"kind\":\"Dab\",\"dabs\":[";
        for (std::size_t index = 0; index < dabs->dabs.size(); ++index) {
            if (index != 0U) result += ',';
            const auto& dab = dabs->dabs[index];
            result += "{\"center\":" + jsonVec(dab.center) + ",\"size\":" + jsonNumber(dab.size) +
                ",\"rotation\":" + jsonNumber(dab.rotation) + ",\"opacity\":" + jsonNumber(dab.opacity) + "}";
        }
        return result + "]}}";
    }
    return result + "{\"kind\":\"Vector\"}}";
}

template <typename Message>
bool serializeCanonical(const Message& source, std::vector<std::uint8_t>& destination) {
    std::string bytes;
    {
        google::protobuf::io::StringOutputStream stream(&bytes);
        google::protobuf::io::CodedOutputStream output(&stream);
        output.SetSerializationDeterministic(true);
        if (!source.SerializeToCodedStream(&output) || output.HadError()) return false;
    }
    destination.assign(bytes.begin(), bytes.end());
    return true;
}
#endif
}

namespace canvas::semantic {

CodecResult SemanticCodec::encodeOperation(OperationKind kind, const std::vector<CanonicalField>& fields) {
    if (!isKnownOperationKind(kind) || fields.size() > 65535U) return {SemanticError::kUnknownOperation, {}};
    std::uint32_t previous = 0;
    bool first = true;
    std::vector<std::uint8_t> out{kMagic0, kMagic1, kVersion, static_cast<std::uint8_t>(kind), 0U, 0U, 0U, 0U};
    appendU16(out, static_cast<std::uint16_t>(fields.size()));
    for (const auto& field : fields) {
        if (field.id == 0U || (!first && field.id <= previous)) {
            return {field.id == previous ? SemanticError::kDuplicateCanonicalKey : SemanticError::kNonCanonicalOrder, {}};
        }
        if (field.bytes.size() > kMaxFieldBytes) return {SemanticError::kLimitExceeded, {}};
        appendU32(out, field.id);
        appendU32(out, static_cast<std::uint32_t>(field.bytes.size()));
        out.insert(out.end(), field.bytes.begin(), field.bytes.end());
        previous = field.id;
        first = false;
    }
    return {SemanticError::kNone, std::move(out)};
}

CodecResult SemanticCodec::encodeProtobufOperation(OperationKind kind) {
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    (void)kind;
    return {SemanticError::kRuntimeUnavailable, {}};
#else
    if (!isKnownOperationKind(kind)) return {SemanticError::kUnknownOperation, {}};
    auditoryworks::axiom::v1::Operation operation;
    operation.set_schema_version(1U);
    operation.set_payload_version(1U);
    operation.mutable_operation_id()->set_value(std::string(16, '\x01'));
    operation.mutable_document_id()->set_value(std::string(16, '\x02'));
    switch (kind) {
        case OperationKind::kInsertObjects: operation.mutable_payload()->mutable_insert_objects(); break;
        case OperationKind::kDeleteObjects: operation.mutable_payload()->mutable_delete_objects(); break;
        case OperationKind::kRestoreObjects: operation.mutable_payload()->mutable_restore_objects(); break;
        case OperationKind::kSetPlacements: operation.mutable_payload()->mutable_set_placements(); break;
        case OperationKind::kSetTransforms: operation.mutable_payload()->mutable_set_transforms(); break;
        case OperationKind::kPatchProperties: operation.mutable_payload()->mutable_patch_properties(); break;
        case OperationKind::kSetObjectSize: operation.mutable_payload()->mutable_set_object_size(); break;
        case OperationKind::kSetVectorPathGeometry: operation.mutable_payload()->mutable_set_vector_path_geometry(); break;
        case OperationKind::kSetImageContent: operation.mutable_payload()->mutable_set_image_content(); break;
        case OperationKind::kAddStroke: operation.mutable_payload()->mutable_add_stroke(); break;
        case OperationKind::kSplitStrokes: operation.mutable_payload()->mutable_split_strokes(); break;
        case OperationKind::kAddEraseMasks: operation.mutable_payload()->mutable_add_erase_masks(); break;
        case OperationKind::kRemoveEraseMasks: operation.mutable_payload()->mutable_remove_erase_masks(); break;
        case OperationKind::kEditRichText: operation.mutable_payload()->mutable_edit_rich_text(); break;
        case OperationKind::kSetConnectorContent: operation.mutable_payload()->mutable_set_connector_content(); break;
    }
    std::string bytes;
    if (!operation.SerializeToString(&bytes)) return {SemanticError::kMalformedWire, {}};
    auditoryworks::axiom::v1::Operation decoded;
    if (!decoded.ParseFromString(bytes) ||
        decoded.payload().payload_case() != operation.payload().payload_case()) {
        return {SemanticError::kMalformedWire, {}};
    }
    return {SemanticError::kNone, {bytes.begin(), bytes.end()}};
#endif
}

DecodedOperation SemanticCodec::decodeOperation(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < kHeaderBytes + 2U) return {{}, {}, SemanticError::kTruncatedWire};
    if (bytes[0] != kMagic0 || bytes[1] != kMagic1) return {{}, {}, SemanticError::kMalformedWire};
    if (bytes[2] != kVersion) return {{}, {}, SemanticError::kUnsupportedVersion};
    const auto kind = static_cast<OperationKind>(bytes[3]);
    if (!isKnownOperationKind(kind)) return {{}, {}, SemanticError::kUnknownOperation};
    const std::uint16_t count = readU16(bytes, kHeaderBytes);
    std::size_t offset = kHeaderBytes + 2U;
    std::uint32_t previous = 0;
    bool first = true;
    std::vector<CanonicalField> fields;
    fields.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        if (bytes.size() - offset < 8U) return {{}, {}, SemanticError::kTruncatedWire};
        const std::uint32_t id = readU32(bytes, offset);
        const std::uint32_t length = readU32(bytes, offset + 4U);
        offset += 8U;
        if (id == 0U || (!first && id <= previous)) return {{}, {}, id == previous ? SemanticError::kDuplicateCanonicalKey : SemanticError::kNonCanonicalOrder};
        if (length > kMaxFieldBytes) return {{}, {}, SemanticError::kLimitExceeded};
        if (bytes.size() - offset < length) return {{}, {}, SemanticError::kTruncatedWire};
        fields.push_back({id, {bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(offset + length)}});
        offset += length;
        previous = id;
        first = false;
    }
    if (offset != bytes.size()) return {{}, {}, SemanticError::kMalformedWire};
    return {Operation{OperationId{}, kind}, std::move(fields), SemanticError::kNone};
}

CodecResult SemanticCodec::encodeCanonicalF64(double value) {
    if (!std::isfinite(value)) {
        return {SemanticError::kNonFiniteValue, {}};
    }
    if (value == 0.0) {
        value = 0.0;
    }
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    std::vector<std::uint8_t> output;
    output.reserve(sizeof(bits));
    for (unsigned shift = 0; shift < 64U; shift += 8U) {
        output.push_back(static_cast<std::uint8_t>(bits >> shift));
    }
    return {SemanticError::kNone, std::move(output)};
}

std::vector<StableSeedCase> SemanticCodec::stableSeedV01() {
    std::vector<StableSeedCase> cases;
    cases.reserve(60U);
    for (unsigned index = 0; index < 60U; ++index) {
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "g1-seed-v0.1-%03u", index);
        cases.push_back({buffer});
    }
    return cases;
}

GoldenCodecObservation SemanticCodec::observeGoldenFixture(
    const std::string& root_type,
    const std::vector<std::uint8_t>& bytes,
    bool strict_canonical) {
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    (void)root_type;
    (void)bytes;
    (void)strict_canonical;
    return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kRuntimeUnavailable, "RUNTIME_UNAVAILABLE", {}};
#else
    const std::string input{bytes.begin(), bytes.end()};
    const auto preflight = preflightCategory(root_type, bytes, strict_canonical);
    if (!preflight.empty()) {
        return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kWirePreflight, preflight, {}};
    }
    std::vector<std::uint8_t> canonical;
    if (root_type == "Id128") {
        auditoryworks::axiom::v1::Id128 value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_value() || !validId(value.value())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "INVALID_ID", {}};
        appendBytes(canonical, 1U, value.value());
    } else if (root_type == "OrderKey") {
        auditoryworks::axiom::v1::OrderKey value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_value() || !validOrderKey(value.value())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVARIANT_VIOLATION", {}};
        appendBytes(canonical, 1U, value.value());
    } else if (root_type == "Vec2") {
        auditoryworks::axiom::v1::Vec2 value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_x() || !value.has_y() || !isFiniteValue(value.x()) || !isFiniteValue(value.y())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        appendFixed64(canonical, 1U, value.x());
        appendFixed64(canonical, 2U, value.y());
    } else if (root_type == "Transform2D") {
        auditoryworks::axiom::v1::Transform2D value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_a() || !value.has_b() || !value.has_c() || !value.has_d() || !value.has_tx() || !value.has_ty() ||
            !isFiniteValue(value.a()) || !isFiniteValue(value.b()) || !isFiniteValue(value.c()) || !isFiniteValue(value.d()) || !isFiniteValue(value.tx()) || !isFiniteValue(value.ty())) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        }
        appendFixed64(canonical, 1U, value.a()); appendFixed64(canonical, 2U, value.b());
        appendFixed64(canonical, 3U, value.c()); appendFixed64(canonical, 4U, value.d());
        appendFixed64(canonical, 5U, value.tx()); appendFixed64(canonical, 6U, value.ty());
    } else if (root_type == "PropertyValue") {
        auditoryworks::axiom::v1::PropertyValue value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_f32_value() || !isFiniteValue(value.f32_value())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        appendFixed32(canonical, 2U, value.f32_value());
    } else if (root_type == "ColorValue") {
        auditoryworks::axiom::v1::ColorValue value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_r() || !value.has_g() || !value.has_b() || !value.has_a() ||
            !isFiniteValue(value.r()) || !isFiniteValue(value.g()) || !isFiniteValue(value.b()) || !isFiniteValue(value.a())) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        }
        appendFixed32(canonical, 1U, value.r()); appendFixed32(canonical, 2U, value.g());
        appendFixed32(canonical, 3U, value.b()); appendFixed32(canonical, 4U, value.a());
    } else if (root_type == "Placement") {
        auditoryworks::axiom::v1::Placement value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_order_key() || !value.order_key().has_value() || !validOrderKey(value.order_key().value())) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVARIANT_VIOLATION", {}};
        }
        if (value.has_parent_id()) {
            if (!value.parent_id().has_value() || !validId(value.parent_id().value())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "INVALID_ID", {}};
            std::vector<std::uint8_t> parent;
            appendBytes(parent, 1U, value.parent_id().value());
            appendMessage(canonical, 1U, parent);
        }
        std::vector<std::uint8_t> order_key;
        appendBytes(order_key, 1U, value.order_key().value());
        appendMessage(canonical, 2U, order_key);
    } else if (root_type == "DashPattern") {
        auditoryworks::axiom::v1::DashPattern value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_offset() || !isFiniteValue(value.offset())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        std::vector<std::uint8_t> packed;
        for (const auto raw_segment : value.segments()) {
            auto segment = raw_segment;
            if (!isFiniteValue(segment)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
            if (segment == 0.0) segment = 0.0;
            const auto bits = std::bit_cast<std::uint64_t>(segment);
            for (unsigned shift = 0; shift < 64U; shift += 8U) packed.push_back(static_cast<std::uint8_t>(bits >> shift));
        }
        appendVarint(canonical, static_cast<std::uint64_t>((1U << 3U) | 2U));
        appendVarint(canonical, packed.size());
        canonical.insert(canonical.end(), packed.begin(), packed.end());
        appendFixed64(canonical, 2U, value.offset());
    } else if (root_type == "DocumentSnapshot") {
        auditoryworks::axiom::v1::DocumentSnapshot value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_document_id() || !value.document_id().has_value() || !validId(value.document_id().value())) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "INVALID_ID", {}};
        }
        std::vector<std::uint8_t> document_id;
        appendBytes(document_id, 1U, value.document_id().value());
        appendMessage(canonical, 1U, document_id);
        appendVarint(canonical, static_cast<std::uint64_t>(2U << 3U));
        appendVarint(canonical, value.schema_version());
    } else if (root_type == "ParagraphStyle") {
        auditoryworks::axiom::v1::ParagraphStyle value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        canvas::semantic::ParagraphStyle mapped;
        if (!mapParagraphStyle(value, mapped) || !serializeCanonical(value, canonical)) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "MISSING_REQUIRED_SEMANTIC_FIELD", {}};
        }
        const auto canonicality = bytes == canonical ? GoldenCanonicality::kCanonical : GoldenCanonicality::kNonCanonicalInput;
        return {true, canonicality, GoldenCodecStage::kCanonicalEncode, {}, std::move(canonical), jsonParagraphStyle(mapped)};
    } else if (root_type == "RichTextDocument") {
        auditoryworks::axiom::v1::RichTextDocument value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        canvas::semantic::RichTextDocument mapped;
        if (!mapRichTextDocument(value, mapped) || !serializeCanonical(value, canonical)) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "MISSING_REQUIRED_SEMANTIC_FIELD", {}};
        }
        const auto canonicality = bytes == canonical ? GoldenCanonicality::kCanonical : GoldenCanonicality::kNonCanonicalInput;
        return {true, canonicality, GoldenCodecStage::kCanonicalEncode, {}, std::move(canonical), jsonRichTextDocument(mapped)};
    } else if (root_type == "RichTextDelta") {
        auditoryworks::axiom::v1::RichTextDelta value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        canvas::semantic::RichTextDelta mapped;
        if (!mapRichTextDelta(value, mapped) || !serializeCanonical(value, canonical)) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "MISSING_REQUIRED_SEMANTIC_FIELD", {}};
        }
        const auto canonicality = bytes == canonical ? GoldenCanonicality::kCanonical : GoldenCanonicality::kNonCanonicalInput;
        return {true, canonicality, GoldenCodecStage::kCanonicalEncode, {}, std::move(canonical), jsonRichTextDelta(mapped)};
    } else if (root_type == "PressureMapping") {
        auditoryworks::axiom::v1::PressureMapping value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        canvas::semantic::PressureMapping mapped;
        if (!mapPressure(value, mapped) || !serializeCanonical(value, canonical)) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "MISSING_REQUIRED_SEMANTIC_FIELD", {}};
        }
        const auto canonicality = bytes == canonical ? GoldenCanonicality::kCanonical : GoldenCanonicality::kNonCanonicalInput;
        return {true, canonicality, GoldenCodecStage::kCanonicalEncode, {}, std::move(canonical), jsonPressure(mapped)};
    } else if (root_type == "BrushDescriptor") {
        auditoryworks::axiom::v1::BrushDescriptor value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        canvas::semantic::BrushDescriptor mapped;
        if (!mapBrush(value, mapped) || !serializeCanonical(value, canonical)) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "MISSING_REQUIRED_SEMANTIC_FIELD", {}};
        }
        const auto canonicality = bytes == canonical ? GoldenCanonicality::kCanonical : GoldenCanonicality::kNonCanonicalInput;
        return {true, canonicality, GoldenCodecStage::kCanonicalEncode, {}, std::move(canonical), jsonBrush(mapped)};
    } else if (root_type == "StrokeRecord") {
        auditoryworks::axiom::v1::StrokeRecord value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        canvas::semantic::StrokeRecord mapped;
        if (!mapStrokeRecord(value, mapped) || !serializeCanonical(value, canonical)) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "MISSING_REQUIRED_SEMANTIC_FIELD", {}};
        }
        const auto canonicality = bytes == canonical ? GoldenCanonicality::kCanonical : GoldenCanonicality::kNonCanonicalInput;
        return {true, canonicality, GoldenCodecStage::kCanonicalEncode, {}, std::move(canonical), jsonStrokeRecord(mapped)};
    } else {
        return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kDtoMap, "UNSUPPORTED_GOLDEN_ROOT", {}};
    }
    const auto canonicality = bytes == canonical ? GoldenCanonicality::kCanonical : GoldenCanonicality::kNonCanonicalInput;
    auto stage = GoldenCodecStage::kCanonicalEncode;
    if (root_type == "Vec2" && canonicality == GoldenCanonicality::kNonCanonicalInput) {
        auditoryworks::axiom::v1::Vec2 normalized;
        if (normalized.ParseFromString(input) && normalized.has_x() && normalized.x() == 0.0 && std::signbit(normalized.x())) {
            stage = GoldenCodecStage::kNormalize;
        }
    }
    return {true, canonicality, stage, {}, std::move(canonical)};
#endif
}

} // namespace canvas::semantic
