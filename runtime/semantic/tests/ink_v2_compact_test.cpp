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
    auto visual=operation;
    auto& visualConfig=std::get<CompactStrokeContent>(std::get<AddStrokeOp>(visual.payload).object.content).stroke.config;
    visualConfig.algorithmVersion=2;visualConfig.materialVersion=2;
    visualConfig.materialResourceId=canvas::foundation::kInkMaterialV2ResourceId;
    visualConfig.materialResourceSha256=canvas::foundation::kInkMaterialV2ResourceSha256;
    const auto visualEncoded=SemanticCodec::encodeProtobufOperation(visual);
    if(!visualEncoded.ok())throw std::runtime_error("visual compact operation rejected");
    const auto visualDecoded=SemanticCodec::decodeProtobufOperation(visualEncoded.bytes);
    if(!visualDecoded.ok()||std::get<CompactStrokeContent>(std::get<AddStrokeOp>(visualDecoded.operation.payload).object.content)!=
        std::get<CompactStrokeContent>(std::get<AddStrokeOp>(visual.payload).object.content))throw std::runtime_error("visual version/material binding changed in codec");
    auto tuned=visual;
    auto& t=std::get<CompactStrokeContent>(std::get<AddStrokeOp>(tuned.payload).object.content).stroke.config;
    t.version=2;t.algorithmVersion=3;t.signalVersion=2;t.thinning=.99;t.streamline=.13;t.smoothing=.61;
    t.easing=canvas::foundation::InkEasing::kEaseOut;t.pressureModel=canvas::foundation::InkPressureModel::kDistance;
    t.startTaper=8;t.endTaper=24;t.startCap=false;t.endCap=true;
    const auto tunedBytes=SemanticCodec::encodeProtobufOperation(tuned);
    if(!tunedBytes.ok())throw std::runtime_error("tuned compact encode");
    const auto tunedDecoded=SemanticCodec::decodeProtobufOperation(tunedBytes.bytes);
    if(!tunedDecoded.ok()||std::get<CompactStrokeContent>(std::get<AddStrokeOp>(tunedDecoded.operation.payload).object.content)!=
       std::get<CompactStrokeContent>(std::get<AddStrokeOp>(tuned.payload).object.content))throw std::runtime_error("tuned controls did not persist");
    t.algorithmVersion=4;t.signalVersion=3;t.endTaper=0;
    const auto qualityBytes=SemanticCodec::encodeProtobufOperation(tuned);
    if(!qualityBytes.ok())throw std::runtime_error("quality compact encode");
    const auto qualityDecoded=SemanticCodec::decodeProtobufOperation(qualityBytes.bytes);
    if(!qualityDecoded.ok()||std::get<CompactStrokeContent>(std::get<AddStrokeOp>(qualityDecoded.operation.payload).object.content)!=
       std::get<CompactStrokeContent>(std::get<AddStrokeOp>(tuned.payload).object.content))throw std::runtime_error("quality evaluator binding changed in codec");
    const auto legacyAgain=SemanticCodec::encodeProtobufOperation(decoded.operation);
    if(!legacyAgain.ok()||legacyAgain.bytes!=encoded.bytes)throw std::runtime_error("legacy compact wire encoding changed");
    t.version=1;if(SemanticCodec::encodeProtobufOperation(tuned).ok())throw std::runtime_error("new tuning accepted as old config");
    auto invalid=operation;
    std::get<CompactStrokeContent>(std::get<AddStrokeOp>(invalid.payload).object.content).stroke.config.algorithmVersion=999;
    if(SemanticCodec::encodeProtobufOperation(invalid).ok())throw std::runtime_error("unknown evaluator accepted");
    invalid=operation;
    auto& config=std::get<CompactStrokeContent>(std::get<AddStrokeOp>(invalid.payload).object.content).stroke.config;
    config.materialResourceSha256="bad-resource-hash";
    if(SemanticCodec::encodeProtobufOperation(invalid).ok())throw std::runtime_error("unknown material resource accepted");
    std::cout << "compact bytes=" << encoded.bytes.size() << '\n';
}
