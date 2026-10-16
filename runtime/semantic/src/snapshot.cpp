#include "canvas/semantic/snapshot.hpp"

#include "object_record_semantics_internal.hpp"
#include "protobuf_codec_internal.hpp"
#include "protobuf_object_mapping.hpp"
#include "sha256_internal.hpp"

#include <algorithm>
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/snapshot.pb.h"
#endif

namespace canvas::semantic {

namespace {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
std::vector<std::uint8_t> canonicalSnapshotBytes(const auditoryworks::axiom::v1::DocumentSnapshot& value) {
    std::vector<std::uint8_t> bytes;
    if (!internal::canonicalEncodeDocumentSnapshot(value).ok()) return {};
    const auto encoded = internal::canonicalEncodeDocumentSnapshot(value);
    return encoded.bytes;
}

std::array<std::uint8_t, 32> snapshotDigest(const auditoryworks::axiom::v1::DocumentSnapshot& source, bool checkpoint) {
    auto value = source;
    value.clear_document_digest();
    value.clear_checkpoint_digest();
    if (!checkpoint) {
        value.clear_document_revision();
        value.clear_recovery_frontier();
        value.clear_digest_algorithm();
    }
    const auto bytes = canonicalSnapshotBytes(value);
    if (!checkpoint) return internal::sha256(bytes);
    std::vector<std::uint8_t> framed;
    constexpr char domain[] = "axiom.checkpoint.v1";
    const auto append64 = [&](std::uint64_t n) { for (unsigned i=0;i<8U;++i) framed.push_back(static_cast<std::uint8_t>(n >> (8U*i))); };
    for (unsigned i=0;i<4U;++i) framed.push_back(static_cast<std::uint8_t>((sizeof(domain)-1U) >> (8U*i)));
    framed.insert(framed.end(), domain, domain + sizeof(domain) - 1U);
    append64(bytes.size());
    framed.insert(framed.end(), bytes.begin(), bytes.end());
    return internal::sha256(framed);
}

bool supportedSnapshotVersion(std::uint32_t version) noexcept {
    return version == 1U || version == 2U || version == 3U;
}

bool snapshotObjectSupported(std::uint32_t schema_version, SnapshotCapability capability, const ObjectRecord& object) noexcept {
    if (object.kind != ObjectKind::kShape || object.kind_version != 2U) return true;
    const auto* shape = std::get_if<ShapeContent>(&object.content);
    return shape == nullptr || shape->shape_kind != 3U ||
           (schema_version == 2U && capability == SnapshotCapability::kShape2Triangle);
}
#endif
}

CodecResult SnapshotCodec::encode(const SemanticSnapshot& snapshot, SnapshotCapability capability) {
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    (void)snapshot;
    (void)capability;
    return {SemanticError::kRuntimeUnavailable, {}};
#else
    if (!supportedSnapshotVersion(snapshot.schema_version)) return {SemanticError::kUnsupportedVersion, {}};
    if (snapshot.document_id.isZero()) return {SemanticError::kInvalidSemanticValue, {}};
    if (snapshot.schema_version < 3U) {
        if (snapshot.manifest_present || snapshot.document_revision != 0U || !snapshot.recovery_frontier.empty() ||
            snapshot.digest_algorithm != 0U || !snapshot.document_digest.empty() ||
            !snapshot.capability_requirements.empty() || !snapshot.checkpoint_digest.empty()) {
            return {SemanticError::kInvalidSemanticValue, {}};
        }
    } else {
        if (!snapshot.manifest_present || !validateResourceManifest(snapshot.manifest) ||
            snapshot.manifest.schema_version != 1U || snapshot.recovery_frontier.empty() ||
            snapshot.digest_algorithm != 1U || snapshot.document_digest.size() != 32U ||
            snapshot.capability_requirements.empty() || snapshot.checkpoint_digest.size() != 32U) {
            return {SemanticError::kInvalidSemanticValue, {}};
        }
    }
    std::vector<ObjectRecord> objects = snapshot.objects;
    for (auto& object : objects) {
        if (!snapshotObjectSupported(snapshot.schema_version, capability, object)) {
            return {SemanticError::kUnsupportedVersion, {}};
        }
        if (!internal::normalizeObjectRecord(object) || !internal::validateObjectRecord(object)) {
            return {SemanticError::kInvalidSemanticValue, {}};
        }
    }
    std::sort(objects.begin(), objects.end(), [](const ObjectRecord& lhs, const ObjectRecord& rhs) {
        return lhs.id.bytes < rhs.id.bytes;
    });
    for (std::size_t i = 1; i < objects.size(); ++i) {
        if (objects[i - 1U].id == objects[i].id) return {SemanticError::kDuplicateCanonicalKey, {}};
    }
    auditoryworks::axiom::v1::DocumentSnapshot wire;
    wire.mutable_document_id()->set_value(std::string(
        reinterpret_cast<const char*>(snapshot.document_id.value().bytes.data()), snapshot.document_id.value().bytes.size()));
    wire.set_schema_version(snapshot.schema_version);
    for (const auto& object : objects) {
        auto* destination = wire.add_objects();
        if (!internal::toProtobufObjectRecord(object, *destination)) return {SemanticError::kInvalidSemanticValue, {}};
    }
    if (snapshot.schema_version >= 3U) {
        if (!internal::toProtobufResourceManifest(snapshot.manifest, *wire.mutable_manifest())) {
            return {SemanticError::kInvalidSemanticValue, {}};
        }
        wire.set_document_revision(snapshot.document_revision);
        wire.set_recovery_frontier(std::string(snapshot.recovery_frontier.begin(), snapshot.recovery_frontier.end()));
        wire.set_digest_algorithm(snapshot.digest_algorithm);
        wire.set_document_digest(std::string(snapshot.document_digest.begin(), snapshot.document_digest.end()));
        wire.set_capability_requirements(std::string(snapshot.capability_requirements.begin(), snapshot.capability_requirements.end()));
        wire.set_checkpoint_digest(std::string(snapshot.checkpoint_digest.begin(), snapshot.checkpoint_digest.end()));
        const auto semantic = snapshotDigest(wire, false);
        const auto checkpoint = snapshotDigest(wire, true);
        const std::vector<std::uint8_t> semantic_bytes(semantic.begin(), semantic.end());
        const std::vector<std::uint8_t> checkpoint_bytes(checkpoint.begin(), checkpoint.end());
        if (snapshot.document_digest != semantic_bytes || snapshot.checkpoint_digest != checkpoint_bytes) {
            return {SemanticError::kInvalidSemanticValue, {}};
        }
    }
    return internal::canonicalEncodeDocumentSnapshot(wire);
#endif
}

SnapshotDecodeResult SnapshotCodec::decode(const std::vector<std::uint8_t>& bytes, SnapshotCapability capability) {
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    (void)bytes;
    (void)capability;
    return {SemanticError::kRuntimeUnavailable, std::nullopt};
#else
    const auto preflight = internal::preflightDocumentSnapshotV1(bytes);
    if (preflight != SemanticError::kNone) return {preflight, std::nullopt};
    auditoryworks::axiom::v1::DocumentSnapshot wire;
    const bool parsed = wire.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()));
    if (!parsed) return {SemanticError::kMalformedWire, std::nullopt};
    if (!wire.has_document_id() || !wire.document_id().has_value() || wire.document_id().value().size() != 16U) {
        return {SemanticError::kInvalidSemanticValue, std::nullopt};
    }
    canvas::foundation::ObjectId raw_document_id;
    std::copy_n(wire.document_id().value().begin(), raw_document_id.bytes.size(), raw_document_id.bytes.begin());
    DocumentId document_id(raw_document_id);
    if (document_id.isZero()) return {SemanticError::kInvalidSemanticValue, std::nullopt};
    if (!supportedSnapshotVersion(wire.schema_version())) return {SemanticError::kUnsupportedVersion, std::nullopt};
    const bool has_extension = wire.has_manifest() || wire.has_document_revision() || wire.has_recovery_frontier() ||
        wire.has_digest_algorithm() || wire.has_document_digest() || wire.has_capability_requirements() || wire.has_checkpoint_digest();
    if (wire.schema_version() < 3U && has_extension) return {SemanticError::kInvalidSemanticValue, std::nullopt};
    if (wire.schema_version() >= 3U && (!wire.has_manifest() || !wire.has_document_revision() ||
        !wire.has_recovery_frontier() || !wire.has_digest_algorithm() || !wire.has_document_digest() ||
        !wire.has_capability_requirements() || !wire.has_checkpoint_digest() ||
        wire.document_digest().size() != 32U || wire.checkpoint_digest().size() != 32U ||
        wire.digest_algorithm() != 1U || wire.capability_requirements().empty() || wire.recovery_frontier().empty())) {
        return {SemanticError::kInvalidSemanticValue, std::nullopt};
    }
    SemanticSnapshot result;
    result.document_id = document_id;
    result.schema_version = wire.schema_version();
    result.manifest_present = wire.has_manifest();
    if (result.manifest_present && !internal::fromProtobufResourceManifest(wire.manifest(), result.manifest)) {
        return {SemanticError::kInvalidSemanticValue, std::nullopt};
    }
    if (wire.schema_version() >= 3U) {
        result.document_revision = wire.document_revision();
        result.recovery_frontier.assign(wire.recovery_frontier().begin(), wire.recovery_frontier().end());
        result.digest_algorithm = wire.digest_algorithm();
        result.document_digest.assign(wire.document_digest().begin(), wire.document_digest().end());
        result.capability_requirements.assign(wire.capability_requirements().begin(), wire.capability_requirements().end());
        result.checkpoint_digest.assign(wire.checkpoint_digest().begin(), wire.checkpoint_digest().end());
        const auto semantic = snapshotDigest(wire, false);
        const auto checkpoint = snapshotDigest(wire, true);
        if (!std::equal(semantic.begin(), semantic.end(), result.document_digest.begin()) ||
            !std::equal(checkpoint.begin(), checkpoint.end(), result.checkpoint_digest.begin())) {
            return {SemanticError::kInvalidSemanticValue, std::nullopt};
        }
    }
    result.objects.reserve(static_cast<std::size_t>(wire.objects_size()));
    for (const auto& object : wire.objects()) {
        ObjectRecord mapped;
        if (!internal::fromProtobufObjectRecord(object, mapped)) return {SemanticError::kInvalidSemanticValue, std::nullopt};
        if (!snapshotObjectSupported(result.schema_version, capability, mapped)) {
            return {SemanticError::kUnsupportedVersion, std::nullopt};
        }
        if (!internal::normalizeObjectRecord(mapped) || !internal::validateObjectRecord(mapped)) {
            return {SemanticError::kInvalidSemanticValue, std::nullopt};
        }
        result.objects.push_back(std::move(mapped));
    }
    std::sort(result.objects.begin(), result.objects.end(), [](const ObjectRecord& lhs, const ObjectRecord& rhs) {
        return lhs.id.bytes < rhs.id.bytes;
    });
    for (std::size_t i = 1; i < result.objects.size(); ++i) {
        if (result.objects[i - 1U].id == result.objects[i].id) return {SemanticError::kDuplicateCanonicalKey, std::nullopt};
    }
    return {SemanticError::kNone, std::move(result)};
#endif
}

} // namespace canvas::semantic
