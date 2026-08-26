#pragma once

#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/semantic_error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace canvas::semantic {

struct CanonicalField final {
    std::uint32_t id = 0;
    std::vector<std::uint8_t> bytes;
};

struct CodecResult final {
    SemanticError error = SemanticError::kNone;
    std::vector<std::uint8_t> bytes;
    [[nodiscard]] bool ok() const noexcept { return error == SemanticError::kNone; }
};

struct DecodedOperation final {
    Operation operation{};
    std::vector<CanonicalField> fields;
    SemanticError error = SemanticError::kNone;
    [[nodiscard]] bool ok() const noexcept { return error == SemanticError::kNone; }
};

struct StableSeedCase final {
    std::string stable_id;
};

class SemanticCodec final {
  public:
    static CodecResult encodeOperation(OperationKind kind, const std::vector<CanonicalField>& fields);
    // Experimental G1-02 probe. Protobuf types remain private to codec.cpp;
    // this returns the canonical runtime bytes without exposing that ABI.
    static CodecResult encodeProtobufOperation(OperationKind kind);
    static DecodedOperation decodeOperation(const std::vector<std::uint8_t>& bytes);
    static CodecResult encodeCanonicalF64(double value);
    static std::vector<StableSeedCase> stableSeedV01();
};

} // namespace canvas::semantic
