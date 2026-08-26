#include "canvas/semantic/codec.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* stageName(canvas::semantic::GoldenCodecStage stage) {
    using canvas::semantic::GoldenCodecStage;
    switch (stage) {
        case GoldenCodecStage::kNone: return "NONE";
        case GoldenCodecStage::kWirePreflight: return "WIRE_PREFLIGHT";
        case GoldenCodecStage::kProtoDecode: return "PROTO_DECODE";
        case GoldenCodecStage::kDtoMap: return "DTO_MAP";
        case GoldenCodecStage::kNormalize: return "NORMALIZE";
        case GoldenCodecStage::kValidate: return "VALIDATE";
        case GoldenCodecStage::kCanonicalEncode: return "CANONICAL_ENCODE";
        case GoldenCodecStage::kRuntimeUnavailable: return "RUNTIME_UNAVAILABLE";
    }
    return "UNKNOWN";
}

const char* canonicalityName(canvas::semantic::GoldenCanonicality canonicality) {
    return canonicality == canvas::semantic::GoldenCanonicality::kCanonical ? "CANONICAL" : "NON_CANONICAL_INPUT";
}

std::string hex(const std::vector<std::uint8_t>& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2U);
    for (const auto byte : bytes) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5 || std::string(argv[1]) != "--root-type" || std::string(argv[3]) != "--input") {
        std::cerr << "usage: canvas_semantic_golden_probe --root-type <type> --input <path>\n";
        return 64;
    }
    std::ifstream stream(argv[4], std::ios::binary);
    if (!stream) {
        std::cerr << "cannot open input\n";
        return 66;
    }
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), {}};
    const auto observation = canvas::semantic::SemanticCodec::observeGoldenFixture(argv[2], bytes, true);
    std::cout << "{\"accepted\":" << (observation.accepted ? "true" : "false")
              << ",\"canonicality\":\"" << canonicalityName(observation.canonicality)
              << "\",\"stage\":\"" << stageName(observation.stage)
              << "\",\"category\":\"" << observation.category
              << "\",\"canonicalHex\":\"" << hex(observation.canonical_bytes) << "\"}\n";
    return 0;
}
