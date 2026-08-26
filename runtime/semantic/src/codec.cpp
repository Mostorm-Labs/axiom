#include "canvas/semantic/codec.hpp"

#include <bit>
#include <cmath>
#include <cstdio>
#include <utility>

#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/operation.pb.h"
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

} // namespace canvas::semantic
