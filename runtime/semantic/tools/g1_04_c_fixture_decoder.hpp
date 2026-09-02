#pragma once

#include "canvas/semantic/operation.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace canvas::verification::g1_04_c {

struct DecodedFixture final {
    std::string caseId;
    std::string operationFamily;
    canvas::semantic::Operation operation{};
    std::vector<canvas::semantic::ObjectRecord> initialObjects;
    std::vector<canvas::semantic::Operation> priorOperations;
    std::vector<std::uint64_t> f64Bits;
};

struct DecodeResult final {
    bool ok = false;
    std::string error;
    std::string errorCode = "HARNESS_ERROR";
    std::optional<DecodedFixture> fixture;
};

struct StringDecodeResult final {
    bool ok = false;
    std::string value;
    std::string error;
};

[[nodiscard]] DecodeResult decodeFixtureJson(std::string_view jsonText);
[[nodiscard]] DecodeResult decodeFixtureFile(const std::string& path);
[[nodiscard]] StringDecodeResult decodeStringScalar(std::string_view value);

} // namespace canvas::verification::g1_04_c
