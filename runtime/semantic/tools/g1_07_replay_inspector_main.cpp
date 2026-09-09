#include "g1_07_replay_inspector.hpp"

#include <cctype>
#include <algorithm>
#include <limits>
#include <iostream>
#include <sstream>

namespace {
using namespace canvas::verification::g1_07;
using canvas::foundation::ObjectId;

void usage() {
    std::cerr << "usage: canvas_semantic_g1_07_replay_inspector "
                 "run|step|seek|object|projection --trace FILE --provider reference|indexed [...]\n";
}

int argumentFailure(const std::string& command, Provider provider) {
    usage();
    ProjectionObservation failure;
    failure.provider = provider == Provider::kReference ? "reference" : "indexed";
    failure.command = command.empty() ? "unknown" : command;
    failure.failure = FailureClass::kTraceInvalid;
    std::cout << writeProjectionJson(failure);
    return 2;
}

bool parseProvider(const std::string& value, Provider& provider) {
    if (value == "reference") { provider = Provider::kReference; return true; }
    if (value == "indexed") { provider = Provider::kIndexed; return true; }
    return false;
}

bool parseUint(const std::string& value, std::size_t& out) {
    if (value.empty() || !std::all_of(value.begin(), value.end(),
                                      [](unsigned char character) { return std::isdigit(character) != 0; })) {
        return false;
    }
    try {
        std::size_t used = 0U;
        const auto parsed = std::stoull(value, &used, 10);
        if (used != value.size() || parsed > std::numeric_limits<std::size_t>::max()) return false;
        out = static_cast<std::size_t>(parsed);
        return true;
    } catch (...) { return false; }
}

bool parseObjectId(const std::string& value, ObjectId& out) {
    if (value.size() != 32U) return false;
    auto nibble = [](char c, std::uint8_t& out) {
        if (c >= '0' && c <= '9') { out = static_cast<std::uint8_t>(c - '0'); return true; }
        if (c >= 'a' && c <= 'f') { out = static_cast<std::uint8_t>(c - 'a' + 10); return true; }
        return false;
    };
    for (std::size_t i = 0U; i < 16U; ++i) {
        std::uint8_t hi = 0U, lo = 0U;
        if (!nibble(value[i * 2U], hi) || !nibble(value[i * 2U + 1U], lo)) return false;
        out.bytes[i] = static_cast<std::uint8_t>((hi << 4U) | lo);
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::string command = argc > 1 ? argv[1] : "unknown";
    if (command != "run" && command != "step" && command != "seek" &&
        command != "object" && command != "projection") {
        return argumentFailure(command, Provider::kReference);
    }
    std::string trace_path;
    Provider provider = Provider::kReference;
    std::size_t position = 0U;
    std::size_t index = 0U;
    ObjectId object_id{};
    bool have_position = false;
    bool have_index = false;
    bool have_object_id = false;
    bool have_trace = false;
    bool have_provider = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--trace" && i + 1 < argc && !have_trace) {
            trace_path = argv[++i];
            have_trace = true;
        } else if (arg == "--provider" && i + 1 < argc && !have_provider) {
            have_provider = true;
            if (!parseProvider(argv[++i], provider)) return argumentFailure(command, provider);
        } else if (arg == "--position" && i + 1 < argc && !have_position) {
            if (!parseUint(argv[++i], position)) return argumentFailure(command, provider);
            have_position = true;
        } else if (arg == "--index" && i + 1 < argc && !have_index) {
            if (!parseUint(argv[++i], index)) return argumentFailure(command, provider);
            have_index = true;
        } else if (arg == "--object-id" && i + 1 < argc && !have_object_id) {
            if (!parseObjectId(argv[++i], object_id)) return argumentFailure(command, provider);
            have_object_id = true;
        } else {
            return argumentFailure(command, provider);
        }
    }
    const bool requires_position = command == "seek" || command == "projection" || command == "object";
    const bool requires_index = command == "step";
    const bool requires_object_id = command == "object";
    if (!have_trace || trace_path.empty() || !have_provider ||
        (requires_position ? !have_position : have_position) ||
        (requires_index ? !have_index : have_index) ||
        (requires_object_id ? !have_object_id : have_object_id)) {
        return argumentFailure(command, provider);
    }
    if (command != "object" && have_object_id) {
        return argumentFailure(command, provider);
    }
    if (command == "run" && (have_position || have_index || have_object_id)) {
        return argumentFailure(command, provider);
    }
    if (command == "step" && (have_position || have_object_id)) {
        return argumentFailure(command, provider);
    }
    if ((command == "seek" || command == "projection") && (have_index || have_object_id)) {
        return argumentFailure(command, provider);
    }
    if (trace_path.empty() || command == "" ||
        ((command == "seek" || command == "projection" || command == "object") && !have_position) ||
        (command == "step" && !have_index) || (command == "object" && !have_object_id)) {
        return argumentFailure(command, provider);
    }
    const auto decoded = readTraceFile(trace_path);
    if (!decoded.ok()) {
        ProjectionObservation failure;
        failure.provider = provider == Provider::kReference ? "reference" : "indexed";
        failure.command = command;
        failure.failure = decoded.failure;
        failure.trace_operation_count = 0U;
        std::cout << writeProjectionJson(failure);
        return decoded.failure == FailureClass::kTraceInvalid ? 2 : 3;
    }
    const ReplayInspector inspector(*decoded.trace, provider);
    if (command == "run") { const auto result = inspector.run(); std::cout << writeRunJson(result); return result.success ? 0 : 3; }
    if (command == "step") { const auto result = inspector.step(index); std::cout << writeStepJson(result); return result.success ? 0 : (result.failure == FailureClass::kPositionInvalid ? 2 : 3); }
    if (command == "seek") { const auto result = inspector.seek(position); std::cout << writeProjectionJson(result); return result.success ? 0 : (result.failure == FailureClass::kPositionInvalid ? 2 : 3); }
    if (command == "projection") { const auto result = inspector.projection(position); std::cout << writeProjectionJson(result); return result.success ? 0 : (result.failure == FailureClass::kPositionInvalid ? 2 : 3); }
    if (command == "object") { const auto result = inspector.object(position, object_id); std::cout << writeObjectJson(result); return result.success ? 0 : (result.failure == FailureClass::kPositionInvalid ? 2 : 3); }
    return argumentFailure(command, provider);
}
