#pragma once
#include "canvas/semantic/operation_payload.hpp"
namespace auditoryworks::axiom::v1 { class OperationPayload; }
namespace canvas::semantic::internal {
bool toProtobufContainerOperation(const OperationPayload&, auditoryworks::axiom::v1::OperationPayload&);
bool fromProtobufContainerOperation(const auditoryworks::axiom::v1::OperationPayload&, OperationPayload&);
}
