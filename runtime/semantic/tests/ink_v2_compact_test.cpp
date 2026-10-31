#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/validator.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    using namespace canvas::semantic;
    Operation operation;
    operation.id=OperationId(ObjectId::fromUint64(11));
    operation.document_id=DocumentId(ObjectId::fromUint64(12));
    operation.schema_version=1;
    operation.payload_version=3;
    ObjectRecord object;
    object.id=ObjectId::fromUint64(13);
    object.kind=ObjectKind::kVectorStroke;
    object.kind_version=3;
    object.placement.order_key=OrderKey({128});
    CompactStrokeContent content;
    content.stroke.config.laserFadeMs=1700;
    content.stroke.config.laserTailLength=420;
    content.stroke.knots={{0,0,32768},{20,0,65535}};
    object.content=content;
    operation.payload=AddStrokeOp{object};
    const auto encoded=SemanticCodec::encodeProtobufOperation(operation);
    if (!encoded.ok()) throw std::runtime_error("compact AddStroke encoding rejected");
    const auto decoded=SemanticCodec::decodeProtobufOperation(encoded.bytes);
    if (!decoded.ok() || std::get<CompactStrokeContent>(std::get<AddStrokeOp>(decoded.operation.payload).object.content)!=content)
        throw std::runtime_error("compact payload did not roundtrip");
    if(encoded.bytes.size()>16384+18*content.stroke.knots.size())
        throw std::runtime_error("compact bytes exceed budget");
    auto invalid=operation;
    std::get<CompactStrokeContent>(std::get<AddStrokeOp>(invalid.payload).object.content).stroke.config.algorithmVersion=999;
    if(SemanticCodec::encodeProtobufOperation(invalid).ok())throw std::runtime_error("unknown evaluator accepted");
    invalid=operation;
    auto& config=std::get<CompactStrokeContent>(std::get<AddStrokeOp>(invalid.payload).object.content).stroke.config;
    config.materialResourceSha256="bad-resource-hash";
    if(SemanticCodec::encodeProtobufOperation(invalid).ok())throw std::runtime_error("unknown material resource accepted");
    std::cout << "compact bytes=" << encoded.bytes.size() << '\n';
}
