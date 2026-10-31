#include "canvas/semantic/snapshot.hpp"

#include "object_record_semantics_internal.hpp"
#include "protobuf_codec_internal.hpp"
#include "protobuf_object_mapping.hpp"
#include "document_background_mapping.hpp"
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
    std::vector<std::uint8_t> framed;
    const auto number=[&](std::uint64_t n,unsigned width){for(unsigned i=0;i<width;++i)framed.push_back(static_cast<std::uint8_t>(n>>(8*i)));};
    const auto blob=[&](const google::protobuf::Message& message){const auto e=internal::canonicalEncodeProto(message);number(e.bytes.size(),4);framed.insert(framed.end(),e.bytes.begin(),e.bytes.end());};
    if(checkpoint) {
        auto value=source;value.clear_checkpoint_digest();
        const auto bytes=canonicalSnapshotBytes(value);
        const std::string domain="axiom.checkpoint.v1";
        number(domain.size(),4);framed.insert(framed.end(),domain.begin(),domain.end());
        number(bytes.size(),8);framed.insert(framed.end(),bytes.begin(),bytes.end());
    } else {
        const std::string domain="axiom.semantic.projection.v1";
        number(domain.size(),4);framed.insert(framed.end(),domain.begin(),domain.end());
        const auto& document=source.document_id().value();framed.insert(framed.end(),document.begin(),document.end());
        number(source.schema_version(),4);number(source.capability_requirements().size(),4);
        framed.insert(framed.end(),source.capability_requirements().begin(),source.capability_requirements().end());
        number(source.has_manifest()?1U:0U,1);number(source.objects_size(),8);
        for(const auto& object:source.objects())blob(object);
        number(source.manifest().schema_version(),4);number(source.manifest().manifest_revision(),8);
        number(source.manifest().entries_size(),8);for(const auto& entry:source.manifest().entries())blob(entry);
        number(source.manifest().retired_entries_size(),8);for(const auto& entry:source.manifest().retired_entries())blob(entry);
        // Absent background retains the released legacy projection byte-for-byte.
        if (source.has_background()) {
            const std::string extension = "axiom.document-background.v1";
            number(extension.size(),4); framed.insert(framed.end(),extension.begin(),extension.end());
            blob(source.background());
        }
    }
    return internal::sha256(framed);
}

struct SnapshotFeatures final { bool triangle=false,text=false,formatting=false,section=false,label=false,connector=false,background=false,table=false; };
SnapshotFeatures snapshotFeatures(const SemanticSnapshot& snapshot) {
    SnapshotFeatures result;
    result.background = snapshot.background != foundation::DocumentBackground{};
    for(const auto& object:snapshot.objects) {
        if(object.kind==ObjectKind::kShape && (object.kind_version==2 || object.kind_version==3)) {
            const auto* shape=std::get_if<ShapeContent>(&object.content);
            result.triangle=result.triangle || (shape && shape->shape_kind==3);
            result.text = result.text || (shape && shape->label.has_value());
            result.label = result.label || (object.kind_version==3 && shape && shape->label.has_value());
        }
        result.text=result.text || (object.kind==ObjectKind::kRichText && object.kind_version>=2);
        result.formatting=result.formatting || (object.kind==ObjectKind::kRichText && object.kind_version==3);
        result.section=result.section || object.kind==ObjectKind::kSection;
        result.table=result.table || object.kind==ObjectKind::kTable;
    }
    for (const auto& object : snapshot.objects) {
        if (object.kind != ObjectKind::kConnector) continue;
        const auto* connector = std::get_if<ConnectorContent>(&object.content);
        if (connector == nullptr) continue;
        for (const auto* endpoint : {&connector->start, &connector->end}) {
            if (const auto* attached = std::get_if<AttachedEndpoint>(&endpoint->value)) {
                const auto target = std::find_if(snapshot.objects.begin(), snapshot.objects.end(),
                    [&](const auto& candidate) { return candidate.id == attached->target_object_id; });
                if (target != snapshot.objects.end() && target->kind == ObjectKind::kShape &&
                    target->kind_version >= 2U) result.connector = true;
            }
        }
    }
    return result;
}
std::vector<std::string> requiredCapabilities(const SnapshotFeatures& features) {
    std::vector<std::string> result{"resource-manifest-v1"};
    if(features.text)result.push_back("richtext-fixed-width-v2");
    if(features.formatting)result.push_back("richtext-formatting-v3");
    if(features.triangle)result.push_back("shape2-triangle-v1");
    if(features.label)result.push_back("shape-richtext-label-v1");
    if(features.connector)result.push_back("connector-shape-attachment-v1");
    if(features.section)result.push_back("section-v1");
    if(features.background)result.push_back("document-background-v1");
    if(features.table)result.push_back("table-v1");
    return result;
}
bool capabilityProfile(const SemanticSnapshot& snapshot,SnapshotCapability reader) {
    const auto bits=static_cast<unsigned>(reader);
    if(bits>127U || !foundation::validDocumentBackground(snapshot.background)) return false;
    const auto features=snapshotFeatures(snapshot);
    if(features.table && ((bits&64U)==0 || snapshot.schema_version<3U))return false;
    if (features.background && ((bits & 32U) == 0 || snapshot.schema_version < 3U)) return false;
    if((features.triangle && (bits&1U)==0) || (features.text && ((bits&2U)==0 || snapshot.schema_version<3))) return false;
    if(features.formatting && (bits&4U)==0)return false;
    if(features.section && ((bits&16U)==0 || snapshot.schema_version<3))return false;
    if(snapshot.schema_version<3) return true;
    if(snapshot.capability_requirements.size()>1024 || snapshot.recovery_frontier.size()>32)return false;
    auditoryworks::axiom::v1::CapabilityRequirementsV1 caps;
    if(!caps.ParseFromArray(snapshot.capability_requirements.data(),static_cast<int>(snapshot.capability_requirements.size())))return false;
    const auto expected=requiredCapabilities(features);
    if (features.connector && (bits & 8U) == 0U) return false;
    if(caps.required_ids_size()!=static_cast<int>(expected.size()))return false;
    for(int i=0;i<caps.required_ids_size();++i)if(caps.required_ids(i)!=expected[static_cast<std::size_t>(i)])return false;
    const auto canonical=internal::canonicalEncodeProto(caps);
    if(!canonical.ok() || canonical.bytes!=snapshot.capability_requirements)return false;
    auditoryworks::axiom::v1::RecoveryFrontierV1 frontier;
    if(!frontier.ParseFromArray(snapshot.recovery_frontier.data(),static_cast<int>(snapshot.recovery_frontier.size())) ||
        !frontier.has_version() || frontier.version()!=1 || !frontier.has_kind() || frontier.kind()!=1 || !frontier.has_sequence())return false;
    const auto encoded=internal::canonicalEncodeProto(frontier);
    if(!encoded.ok() || encoded.bytes!=snapshot.recovery_frontier)return false;
    for(const auto& object:snapshot.objects) {
        const RichTextContent* rich = nullptr;
        if (object.kind == ObjectKind::kRichText && object.kind_version >= 2) rich = std::get_if<RichTextContent>(&object.content);
        else if (object.kind == ObjectKind::kShape && object.kind_version == 3) { const auto* shape = std::get_if<ShapeContent>(&object.content); if (shape && shape->label) rich = &*shape->label; }
        if(const auto* table=std::get_if<TableContent>(&object.content))for(const auto& c:table->cells)for(const auto& p:c.text.document.paragraphs)for(const auto& r:p.runs){if(!r.style.font_resource_id||std::none_of(snapshot.manifest.entries.begin(),snapshot.manifest.entries.end(),[&](const auto& e){return e.resource_id==*r.style.font_resource_id&&e.resource_kind==ResourceKind::kFont;}))return false;}
        if (!rich) continue;
        for(const auto& paragraph:rich->document.paragraphs) for(const auto& run:paragraph.runs) {
            if(!run.style.font_resource_id)return false;
            const auto found=std::find_if(snapshot.manifest.entries.begin(),snapshot.manifest.entries.end(),[&](const auto& e){return e.resource_id==*run.style.font_resource_id && e.resource_kind==ResourceKind::kFont;});
            if(found==snapshot.manifest.entries.end())return false;
        }
    }
    return true;
}

bool supportedSnapshotVersion(std::uint32_t version) noexcept {
    return version == 1U || version == 2U || version == 3U;
}

bool snapshotObjectSupported(std::uint32_t schema_version, SnapshotCapability capability, const ObjectRecord& object) noexcept {
    if (object.kind == ObjectKind::kTable) return schema_version==3U && (static_cast<unsigned>(capability)&64U)!=0;
    if (object.kind == ObjectKind::kSection)
        return schema_version == 3U && (static_cast<unsigned>(capability) & 16U) != 0U;
    if (object.kind == ObjectKind::kRichText && object.kind_version == 3U)
        return schema_version==3U && (static_cast<unsigned>(capability)&6U)==6U;
    if (object.kind == ObjectKind::kRichText && object.kind_version == 2U)
        return schema_version==3U && (static_cast<unsigned>(capability)&2U)!=0U;
    if (object.kind != ObjectKind::kShape || object.kind_version != 2U) return true;
    const auto* shape = std::get_if<ShapeContent>(&object.content);
    return shape == nullptr || shape->shape_kind != 3U ||
           ((schema_version == 2U || schema_version == 3U) && (static_cast<unsigned>(capability)&1U)!=0U);
}
#endif
}

static CodecResult encodeSnapshot(const SemanticSnapshot& snapshot, SnapshotCapability capability, SemanticSnapshot* binding) {
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    (void)snapshot;
    (void)capability;(void)binding;
    return {SemanticError::kRuntimeUnavailable, {}};
#else
    if (!supportedSnapshotVersion(snapshot.schema_version)) return {SemanticError::kUnsupportedVersion, {}};
    if (snapshot.document_id.isZero() || !capabilityProfile(snapshot,capability)) return {SemanticError::kInvalidSemanticValue, {}};
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
    if (snapshot.background != foundation::DocumentBackground{})
        internal::toProtobufBackground(snapshot.background, *wire.mutable_background());
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
        if(binding) {
            wire.set_document_digest(std::string(semantic.begin(),semantic.end()));
            const auto bound_checkpoint=snapshotDigest(wire,true);
            binding->document_digest=semantic_bytes;
            binding->checkpoint_digest.assign(bound_checkpoint.begin(),bound_checkpoint.end());
            wire.set_checkpoint_digest(std::string(bound_checkpoint.begin(),bound_checkpoint.end()));
        } else if (snapshot.document_digest != semantic_bytes || snapshot.checkpoint_digest != checkpoint_bytes) {
            return {SemanticError::kInvalidSemanticValue, {}};
        }
    }
    return internal::canonicalEncodeDocumentSnapshot(wire);
#endif
}

CodecResult SnapshotCodec::encode(const SemanticSnapshot& snapshot,SnapshotCapability capability) { return encodeSnapshot(snapshot,capability,nullptr); }
bool SnapshotCodec::populateRequirements(SemanticSnapshot& snapshot) {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
    if (snapshot.schema_version != 3) return false;
    auditoryworks::axiom::v1::CapabilityRequirementsV1 requirements;
    for (const auto& name : requiredCapabilities(snapshotFeatures(snapshot))) requirements.add_required_ids(name);
    const auto encoded = internal::canonicalEncodeProto(requirements);
    if (!encoded.ok()) return false;
    snapshot.capability_requirements = encoded.bytes;
    return true;
#else
    (void)snapshot; return false;
#endif
}
bool SnapshotCodec::bindDigests(SemanticSnapshot& snapshot,SnapshotCapability capability) {
    if(snapshot.schema_version!=3)return false;
    auto staged=snapshot;staged.document_digest.resize(32);staged.checkpoint_digest.resize(32);
    if(!encodeSnapshot(staged,capability,&staged).ok())return false;
    snapshot=std::move(staged);return true;
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
        wire.has_digest_algorithm() || wire.has_document_digest() || wire.has_capability_requirements() || wire.has_checkpoint_digest() || wire.has_background();
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
    if (wire.has_background() &&
        (!internal::fromProtobufBackground(wire.background(), result.background) ||
         result.background == foundation::DocumentBackground{}))
        return {SemanticError::kInvalidSemanticValue, std::nullopt};
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
    if(!capabilityProfile(result,capability))return {SemanticError::kUnsupportedVersion,std::nullopt};
    return {SemanticError::kNone, std::move(result)};
#endif
}

} // namespace canvas::semantic
