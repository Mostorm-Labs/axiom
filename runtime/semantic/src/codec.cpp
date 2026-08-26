#include "canvas/semantic/codec.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <utility>

#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/common.pb.h"
#include "auditoryworks/axiom/v1/geometry.pb.h"
#include "auditoryworks/axiom/v1/operation.pb.h"
#include "auditoryworks/axiom/v1/object.pb.h"
#include "auditoryworks/axiom/v1/paint.pb.h"
#include "auditoryworks/axiom/v1/property.pb.h"
#include "auditoryworks/axiom/v1/snapshot.pb.h"
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
        fields.push_back({number, type});
        switch (type) {
            case 0U: {
                std::uint64_t value = 0U;
                if (!readVarint(bytes, offset, value)) return false;
                break;
            }
            case 1U:
                if (bytes.size() - offset < 8U) return false;
                offset += 8U;
                break;
            case 2U: {
                std::uint64_t length = 0U;
                if (!readVarint(bytes, offset, length) || length > bytes.size() - offset) return false;
                offset += static_cast<std::size_t>(length);
                break;
            }
            case 5U:
                if (bytes.size() - offset < 4U) return false;
                offset += 4U;
                break;
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
    return false;
}

std::string preflightCategory(const std::string& root_type, const std::vector<std::uint8_t>& bytes, bool strict_canonical) {
    std::vector<WireField> fields;
    if (!scanWire(bytes, fields)) return "MALFORMED_WIRE";
    std::vector<std::uint32_t> seen;
    unsigned property_oneof_members = 0U;
    for (const auto& field : fields) {
        if (!allowsField(root_type, field.number)) return "UNKNOWN_WIRE_FIELD";
        if (root_type == "DashPattern" && field.number == 1U && field.type != 2U && strict_canonical) {
            return "NON_CANONICAL_PACKED_ENCODING";
        }
        if (root_type == "PropertyValue" && field.number >= 1U && field.number <= 7U) {
            ++property_oneof_members;
        }
        const bool repeated_dash_segment = root_type == "DashPattern" && field.number == 1U && field.type == 1U;
        if (!repeated_dash_segment && std::find(seen.begin(), seen.end(), field.number) != seen.end()) {
            return "DUPLICATE_SINGULAR_FIELD";
        }
        seen.push_back(field.number);
    }
    if (root_type == "PropertyValue" && property_oneof_members > 1U) return "MULTIPLE_ONEOF_MEMBERS";
    return {};
}

bool finite(double value) { return std::isfinite(value); }

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
    return {Operation{kind}, std::move(fields), SemanticError::kNone};
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
        if (!value.has_x() || !value.has_y() || !finite(value.x()) || !finite(value.y())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        appendFixed64(canonical, 1U, value.x());
        appendFixed64(canonical, 2U, value.y());
    } else if (root_type == "Transform2D") {
        auditoryworks::axiom::v1::Transform2D value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_a() || !value.has_b() || !value.has_c() || !value.has_d() || !value.has_tx() || !value.has_ty() ||
            !finite(value.a()) || !finite(value.b()) || !finite(value.c()) || !finite(value.d()) || !finite(value.tx()) || !finite(value.ty())) {
            return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        }
        appendFixed64(canonical, 1U, value.a()); appendFixed64(canonical, 2U, value.b());
        appendFixed64(canonical, 3U, value.c()); appendFixed64(canonical, 4U, value.d());
        appendFixed64(canonical, 5U, value.tx()); appendFixed64(canonical, 6U, value.ty());
    } else if (root_type == "PropertyValue") {
        auditoryworks::axiom::v1::PropertyValue value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_f32_value() || !finite(value.f32_value())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        appendFixed32(canonical, 2U, value.f32_value());
    } else if (root_type == "ColorValue") {
        auditoryworks::axiom::v1::ColorValue value;
        if (!value.ParseFromString(input)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kProtoDecode, "MALFORMED_WIRE", {}};
        if (!value.has_r() || !value.has_g() || !value.has_b() || !value.has_a() ||
            !finite(value.r()) || !finite(value.g()) || !finite(value.b()) || !finite(value.a())) {
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
        if (!value.has_offset() || !finite(value.offset())) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
        std::vector<std::uint8_t> packed;
        for (const auto raw_segment : value.segments()) {
            auto segment = raw_segment;
            if (!finite(segment)) return {false, GoldenCanonicality::kCanonical, GoldenCodecStage::kValidate, "INVALID_NUMERIC", {}};
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
