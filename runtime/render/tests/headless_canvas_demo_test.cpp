#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "canvas/semantic/snapshot.hpp"

namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<std::uint8_t>;

Bytes bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

std::uint64_t fnv(const Bytes& value) {
    std::uint64_t result = 1469598103934665603ULL;
    for (const auto byte : value) {
        result ^= byte;
        result *= 1099511628211ULL;
    }
    return result;
}

std::string text(const fs::path& path) {
    const auto value = bytes(path);
    return {value.begin(), value.end()};
}

std::string sha256(const fs::path& path) {
    const std::string command = "shasum -a 256 '" + path.string() + "'";
    FILE* pipe = popen(command.c_str(), "r");
    assert(pipe != nullptr);
    char buffer[256]{};
    const auto count = fread(buffer, 1, sizeof(buffer) - 1U, pipe);
    assert(pclose(pipe) == 0);
    std::istringstream stream(std::string(buffer, count));
    std::string digest;
    stream >> digest;
    return digest;
}

std::string field(const std::string& json, const std::string& key) {
    const auto marker = "\"" + key + "\":";
    const auto begin = json.find(marker);
    assert(begin != std::string::npos);
    auto value_begin = begin + marker.size();
    while (value_begin < json.size() &&
           (json[value_begin] == ' ' || json[value_begin] == '\n' ||
            json[value_begin] == '\r' || json[value_begin] == '\t')) {
        ++value_begin;
    }
    assert(value_begin < json.size());
    if (json[value_begin] == '"') {
        const auto end = json.find('"', value_begin + 1U);
        assert(end != std::string::npos);
        return json.substr(value_begin, end - value_begin + 1U);
    }
    if (json[value_begin] == '[' || json[value_begin] == '{') {
        const char open = json[value_begin];
        const char close = open == '[' ? ']' : '}';
        int depth = 0;
        bool quoted = false;
        for (std::size_t index = value_begin; index < json.size(); ++index) {
            if (json[index] == '"' && (index == 0U || json[index - 1U] != '\\')) quoted = !quoted;
            if (quoted) continue;
            if (json[index] == open) ++depth;
            if (json[index] == close && --depth == 0) return json.substr(value_begin, index - value_begin + 1U);
        }
        assert(false);
    }
    const auto end = json.find_first_of(",}\n", value_begin);
    assert(end != std::string::npos);
    return json.substr(value_begin, end - value_begin);
}

std::string compactJson(const std::string& json) {
    std::string result;
    bool quoted = false;
    for (std::size_t index = 0; index < json.size(); ++index) {
        if (json[index] == '"' && (index == 0U || json[index - 1U] != '\\')) quoted = !quoted;
        if (quoted || (json[index] != ' ' && json[index] != '\n' && json[index] != '\r' && json[index] != '\t')) {
            result.push_back(json[index]);
        }
    }
    return result;
}

std::array<std::uint8_t, 4> pixel(const Bytes& value, std::size_t x, std::size_t y) {
    const auto index = (y * 256U + x) * 4U;
    return {value[index], value[index + 1U], value[index + 2U], value[index + 3U]};
}

void expectFailure(const std::string& binary, const std::string& arguments, const fs::path& output) {
    std::error_code error;
    fs::remove_all(output, error);
    assert(std::system((binary + arguments).c_str()) != 0);
    assert(!fs::exists(output));
}

void assertDescriptor(const std::string& expected, const std::string& evidence) {
    for (const auto& key : {"fixture_id", "fixture_sha256", "object_ids", "object_kinds"}) {
        const auto expected_value = compactJson(field(expected, key));
        const auto evidence_value = compactJson(field(evidence, key));
        if (expected_value != evidence_value) {
            std::fprintf(stderr, "descriptor mismatch %s: %s != %s\n", key, expected_value.c_str(), evidence_value.c_str());
            assert(false);
        }
    }
    assert(compactJson(field(expected, "raster")) == compactJson(field(evidence, "raster")));
    assert(compactJson(field(expected, "vector_stroke")) == compactJson(field(evidence, "vector_stroke")));
    assert(compactJson(field(expected, "group")) == compactJson(field(evidence, "group")));
}

void assertStageCorrelation(const std::string& evidence) {
    assert(field(evidence, "canonical_ids") == "[501,502,503,504,505,506,507,508,509]");
    assert(field(evidence, "runtime_ids") == "[501,502,503,504,505,506,507,508,509]");
    assert(field(evidence, "scene_ids") == "[501,502,503,504,505,506,507,508,509]");
    assert(field(evidence, "visibility_ids") == "[501,502,503,504,505,506,507,508]");
    assert(field(evidence, "draw_ids") == "[501,502,503,504,505,506,507,508]");
    assert(field(evidence, "reference_ids") == "[501,502,503,504,505,506,507,508]");
    assert(field(evidence, "backend_ids") == "[501,502,503,504,505,506,507,508]");
    assert(field(evidence, "canonical_kinds") ==
           "[\"Shape\",\"Image\",\"VectorPath\",\"RichText\",\"VectorStroke\",\"DabStroke\",\"Connector\",\"Sticky\",\"Group\"]");
    assert(field(evidence, "runtime_kinds") == field(evidence, "canonical_kinds"));
    assert(field(evidence, "scene_kinds") ==
           "[\"Shape\",\"Image\",\"VectorPath\",\"RichText\",\"VectorStroke\",\"DabStroke\",\"Shape\",\"Shape\",\"Shape\"]");
    assert(field(evidence, "reference_kinds") ==
           "[\"Shape\",\"Image\",\"VectorPath\",\"RichText\",\"VectorStroke\",\"DabStroke\",\"Connector\",\"Sticky\"]");
    assert(field(evidence, "backend_kinds") == field(evidence, "reference_kinds"));
    assert(field(evidence, "runtime_group_findable") == "true");
    assert(field(evidence, "group_absent_from_pixel_stages") == "true");
}

} // namespace

int main() {
    const fs::path root = G3_07_SOURCE_ROOT;
    const fs::path fixture = root / "runtime/render/fixtures/g3-07/nine-kind-v1.axsnap";
    const fs::path expected = root / "runtime/render/fixtures/g3-07/nine-kind-v1.expected.json";
    assert(fs::exists(fixture) && fs::exists(expected));

    const auto fixture_bytes = bytes(fixture);
    assert(fixture_bytes.size() == 1791U);
    assert(fnv(fixture_bytes) == 0xc74f5632133e9de2ULL);
    assert(sha256(fixture) == "b4d04c4424c50370991729bce124c48e8c822487c788de6d535e959aab27c37f");

    const auto decoded = canvas::semantic::SnapshotCodec::decode(fixture_bytes);
    assert(decoded.ok() && decoded.snapshot->objects.size() == 9U);
    for (std::uint64_t index = 0; index < 9U; ++index) {
        assert(decoded.snapshot->objects[index].id == canvas::foundation::ObjectId::fromUint64(501U + index));
        assert(static_cast<unsigned>(decoded.snapshot->objects[index].kind) == index + 1U);
    }

    const fs::path scratch = fs::temp_directory_path() / "axiom-g3-07-demo-test";
    std::error_code error;
    fs::remove_all(scratch, error);
    fs::create_directories(scratch, error);
    const std::string binary = G3_07_DEMO_BINARY;
    const std::string fixture_arg = fixture.string();
    const fs::path first = scratch / "first";
    const fs::path second = scratch / "second";

    assert(std::system((binary + " --fixture '" + fixture_arg + "' --output-dir '" + first.string() + "'").c_str()) == 0);
    assert(std::system((binary + " --fixture '" + fixture_arg + "' --output-dir '" + second.string() + "'").c_str()) == 0);
    for (const char* name : {"render.rgba", "render-digest.txt", "render-evidence.json"}) {
        assert(bytes(first / name) == bytes(second / name));
    }

    const auto raster = bytes(first / "render.rgba");
    assert(raster.size() == 256U * 256U * 4U);
    assert(sha256(first / "render.rgba") == "81d2314034d0fcb0152daa33f5706ac8d519c1affba9c646c2b99145bfbdf042");
    assert(text(first / "render-digest.txt") == "fnv1a64:7b8eb8c70c65afe5\n");

    const auto expected_text = text(expected);
    const auto evidence_text = text(first / "render-evidence.json");
    assertDescriptor(expected_text, evidence_text);
    assertStageCorrelation(evidence_text);
    assert(field(evidence_text, "pipeline_stages") ==
           "[\"SnapshotCodec.decode\",\"SnapshotBootstrapper.restore\",\"IndexedObjectStore\",\"FullSceneCompiler\",\"Scene\",\"RuntimeScene\",\"VisibilityResolver\",\"DirectReferenceSource\",\"FramePlanBuilder\",\"FrameOrchestrator\",\"SkiaHeadlessBackend\"]");

    const auto magenta = std::array<std::uint8_t, 4>{255, 0, 255, 255};
    const auto transparent = std::array<std::uint8_t, 4>{0, 0, 0, 0};
    assert(pixel(raster, 8, 46) == magenta);
    assert(pixel(raster, 16, 46) == transparent);
    assert(pixel(raster, 24, 46) == magenta);
    for (std::size_t y = 0; y < 256U; ++y) {
        for (std::size_t x = 128U; x < 256U; ++x) assert(pixel(raster, x, y)[3] == 0U);
    }

    const fs::path existing = scratch / "existing";
    assert(std::system((binary + " --fixture '" + fixture_arg + "' --output-dir '" + existing.string() + "'").c_str()) == 0);
    const auto existing_before = bytes(existing / "render.rgba");
    assert(std::system((binary + " --fixture '" + fixture_arg + "' --output-dir '" + existing.string() + "'").c_str()) != 0);
    assert(bytes(existing / "render.rgba") == existing_before);

    const fs::path clean_output = scratch / "negative-output";
    expectFailure(binary, "", clean_output);
    expectFailure(binary, " --unknown x", clean_output);
    expectFailure(binary, " --fixture '" + fixture_arg + "'", clean_output);
    expectFailure(binary, " --output-dir '" + clean_output.string() + "'", clean_output);
    expectFailure(binary, " --fixture --output-dir '" + clean_output.string() + "'", clean_output);
    expectFailure(binary, " --fixture '" + fixture_arg + "' --output-dir", clean_output);
    expectFailure(binary, " --fixture '" + fixture_arg + "' --fixture '" + fixture_arg + "' --output-dir '" + clean_output.string() + "'", clean_output);
    expectFailure(binary, " --fixture '" + fixture_arg + "' --output-dir '" + clean_output.string() + "' --output-dir '" + clean_output.string() + "'", clean_output);
    expectFailure(binary, " --fixture '" + fixture_arg + "' --output-dir '" + clean_output.string() + "' positional", clean_output);

    const fs::path malformed = scratch / "malformed.axsnap";
    std::ofstream(malformed) << "not-a-snapshot";
    expectFailure(binary, " --fixture '" + malformed.string() + "' --output-dir '" + clean_output.string() + "'", clean_output);
    const fs::path missing = scratch / "missing.axsnap";
    expectFailure(binary, " --fixture '" + missing.string() + "' --output-dir '" + clean_output.string() + "'", clean_output);
    const fs::path unsupported = scratch / "unsupported.axsnap";
    auto unsupported_bytes = fixture_bytes;
    bool changed_schema = false;
    for (std::size_t index = 0; index + 1U < unsupported_bytes.size(); ++index) {
        if (unsupported_bytes[index] == 0x10U && unsupported_bytes[index + 1U] == 0x01U) {
            unsupported_bytes[index + 1U] = 0x02U;
            changed_schema = true;
            break;
        }
    }
    assert(changed_schema);
    std::ofstream(unsupported, std::ios::binary).write(reinterpret_cast<const char*>(unsupported_bytes.data()), static_cast<std::streamsize>(unsupported_bytes.size()));
    expectFailure(binary, " --fixture '" + unsupported.string() + "' --output-dir '" + clean_output.string() + "'", clean_output);

    const fs::path invalid = scratch / "invalid-canonical.axsnap";
    auto invalid_bytes = fixture_bytes;
    assert(invalid_bytes.size() > 20U && invalid_bytes[0] == 0x0aU && invalid_bytes[2] == 0x0aU && invalid_bytes[3] == 0x10U);
    std::fill(invalid_bytes.begin() + 4, invalid_bytes.begin() + 20, 0U);
    std::ofstream(invalid, std::ios::binary).write(reinterpret_cast<const char*>(invalid_bytes.data()), static_cast<std::streamsize>(invalid_bytes.size()));
    expectFailure(binary, " --fixture '" + invalid.string() + "' --output-dir '" + clean_output.string() + "'", clean_output);

    auto nonintegral_snapshot = *decoded.snapshot;
    auto& shape_content = std::get<canvas::semantic::ShapeContent>(nonintegral_snapshot.objects.front().content);
    shape_content.width = 1.5;
    const auto nonintegral_encoded = canvas::semantic::SnapshotCodec::encode(nonintegral_snapshot);
    assert(nonintegral_encoded.ok());
    const fs::path nonintegral = scratch / "nonintegral.axsnap";
    std::ofstream(nonintegral, std::ios::binary).write(reinterpret_cast<const char*>(nonintegral_encoded.bytes.data()), static_cast<std::streamsize>(nonintegral_encoded.bytes.size()));
    expectFailure(binary, " --fixture '" + nonintegral.string() + "' --output-dir '" + clean_output.string() + "'", clean_output);

    fs::remove_all(scratch, error);
    return 0;
}
