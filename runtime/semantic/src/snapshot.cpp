#include "canvas/semantic/snapshot.hpp"

#include "protobuf_object_mapping.hpp"

#include <algorithm>
#include <string>

#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/snapshot.pb.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#endif

namespace canvas::semantic {

CodecResult SnapshotCodec::encode(const SemanticSnapshot& snapshot) {
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    (void)snapshot;
    return {SemanticError::kRuntimeUnavailable, {}};
#else
    if (snapshot.schema_version != 1U) return {SemanticError::kUnsupportedVersion, {}};
    if (snapshot.document_id.isZero()) return {SemanticError::kInvalidSemanticValue, {}};
    std::vector<ObjectRecord> objects = snapshot.objects;
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
    std::string bytes;
    {
        google::protobuf::io::StringOutputStream stream(&bytes);
        google::protobuf::io::CodedOutputStream output(&stream);
        output.SetSerializationDeterministic(true);
        if (!wire.SerializeToCodedStream(&output) || output.HadError()) return {SemanticError::kMalformedWire, {}};
    }
    return {SemanticError::kNone, std::vector<std::uint8_t>(bytes.begin(), bytes.end())};
#endif
}

SnapshotDecodeResult SnapshotCodec::decode(const std::vector<std::uint8_t>& bytes) {
#if !defined(CANVAS_SEMANTIC_PROTOBUF)
    (void)bytes;
    return {SemanticError::kRuntimeUnavailable, std::nullopt};
#else
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
    if (wire.schema_version() != 1U) return {SemanticError::kUnsupportedVersion, std::nullopt};
    SemanticSnapshot result;
    result.document_id = document_id;
    result.schema_version = wire.schema_version();
    result.objects.reserve(static_cast<std::size_t>(wire.objects_size()));
    for (const auto& object : wire.objects()) {
        ObjectRecord mapped;
        if (!internal::fromProtobufObjectRecord(object, mapped)) return {SemanticError::kInvalidSemanticValue, std::nullopt};
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
