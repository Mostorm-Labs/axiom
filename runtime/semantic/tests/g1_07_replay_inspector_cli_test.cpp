#include "g1_07_replay_corpus.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace canvas::verification::g1_07 {

#if defined(G1_07_REPLAY_INSPECTOR_BINARY_PATH)
namespace {

struct ProcessResult final {
    int exit_code = -1;
    std::string stdout_text;
    std::string stderr_text;
};

std::string quoteForShell(const std::string& value) {
    std::string quoted{"'"};
    for (const char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    quoted += '\'';
    return quoted;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int normalizedExitCode(int status) {
#if defined(_WIN32)
    return status;
#else
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return status;
#endif
}

class CliFixture final {
  public:
    CliFixture() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("axiom-g1-07-cli-" + std::to_string(nonce));
        std::filesystem::create_directories(root_);
        trace_ = root_ / "empty.trace.json";
        std::ofstream output(trace_);
        output << R"({
  "format": "axiom-semantic-replay-trace-v1",
  "formatVersion": 1,
  "documentIdHex": "06070000000000000000000000000000",
  "schemaVersion": 1,
  "baseline": {
    "kind": "empty",
    "semanticGeneration": 0,
    "runtimeEpoch": 42,
    "commitOrdinal": 0
  },
  "operations": []
})";
    }

    ~CliFixture() { std::filesystem::remove_all(root_); }

    ProcessResult run(const std::vector<std::string>& arguments) const {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto stdout_path = root_ / ("stdout-" + std::to_string(nonce) + ".txt");
        const auto stderr_path = root_ / ("stderr-" + std::to_string(nonce) + ".txt");
        std::string command = quoteForShell(G1_07_REPLAY_INSPECTOR_BINARY_PATH);
        for (const auto& argument : arguments) command += " " + quoteForShell(argument);
        command += " >" + quoteForShell(stdout_path.string()) +
                   " 2>" + quoteForShell(stderr_path.string());
        const int status = std::system(command.c_str());
        ProcessResult result;
        result.exit_code = normalizedExitCode(status);
        result.stdout_text = readFile(stdout_path);
        result.stderr_text = readFile(stderr_path);
        std::filesystem::remove(stdout_path);
        std::filesystem::remove(stderr_path);
        return result;
    }

    [[nodiscard]] std::string tracePath() const { return trace_.string(); }

  private:
    std::filesystem::path root_;
    std::filesystem::path trace_;
};

nlohmann::ordered_json parseOutput(const ProcessResult& result) {
    if (result.stdout_text.empty()) {
        ADD_FAILURE() << "CLI stdout must contain exactly one JSON document";
        return nlohmann::ordered_json::object();
    }
    EXPECT_EQ(result.stdout_text.back(), '\n');
    return nlohmann::ordered_json::parse(result.stdout_text);
}

void expectEnvelope(const nlohmann::ordered_json& output, const char* command,
                    std::size_t operation_count = 0U) {
    EXPECT_EQ(output.at("format"), "axiom-semantic-replay-inspector-output-v1");
    EXPECT_EQ(output.at("formatVersion"), 1U);
    EXPECT_EQ(output.at("command"), command);
    EXPECT_EQ(output.at("traceOperationCount"), operation_count);
    EXPECT_TRUE(output.contains("success"));
    EXPECT_TRUE(output.contains("failure"));
}

} // namespace
#endif

TEST(G107ReplayInspectorCli, OutputUsesStableMachineReadableEnvelope) {
    const ReplayInspector inspector(makeMinimumTrace(), Provider::kReference);
    const auto output = writeRunJson(inspector.run());
    EXPECT_NE(output.find("axiom-semantic-replay-inspector-output-v1"), std::string::npos);
    EXPECT_EQ(output.back(), '\n');
    const auto json = nlohmann::ordered_json::parse(output);
    EXPECT_EQ(json.at("provider"), "reference");
    EXPECT_EQ(json.at("command"), "run");
    EXPECT_EQ(json.at("traceOperationCount"), 0U);
    EXPECT_EQ(json.at("requestedCursor"), 0U);
    EXPECT_EQ(json.at("resultingCursor"), 0U);
    EXPECT_EQ(json.at("digestHex").get<std::string>().size(), 16U);
    EXPECT_TRUE(json.at("projection").is_object());
    EXPECT_FALSE(json.contains("revision"));
}

TEST(G107ReplayInspectorCli, InvalidPositionUsesDefinedDiagnostic) {
    const ReplayInspector inspector(makeMinimumTrace(), Provider::kReference);
    const auto output = writeProjectionJson(inspector.projection(1U));
    EXPECT_NE(output.find("POSITION_INVALID"), std::string::npos);
}

TEST(G107ReplayInspectorCli, EveryCommandUsesOneDeterministicSchema) {
    const ReplayInspector inspector(makeAllOperationFamiliesTrace(), Provider::kIndexed);
    const auto run = nlohmann::ordered_json::parse(writeRunJson(inspector.run()));
    const auto step = nlohmann::ordered_json::parse(writeStepJson(inspector.step(0U)));
    const auto seek = nlohmann::ordered_json::parse(writeProjectionJson(inspector.seek(1U)));
    const auto object = nlohmann::ordered_json::parse(writeObjectJson(
        inspector.object(1U, canvas::foundation::ObjectId::fromUint64(100U))));
    const auto projection = nlohmann::ordered_json::parse(writeProjectionJson(inspector.projection(1U)));

    const std::array<std::reference_wrapper<const nlohmann::ordered_json>, 5> outputs{
        std::cref(run), std::cref(step), std::cref(seek), std::cref(object), std::cref(projection)};
    const std::array<const char*, 5> commands{"run", "step", "seek", "object", "projection"};
    for (std::size_t index = 0U; index < outputs.size(); ++index) {
        const auto& output = outputs[index].get();
        EXPECT_EQ(output.at("format"), "axiom-semantic-replay-inspector-output-v1");
        EXPECT_EQ(output.at("formatVersion"), 1U);
        EXPECT_EQ(output.at("provider"), "indexed");
        EXPECT_EQ(output.at("command"), commands[index]);
        EXPECT_EQ(output.at("traceOperationCount"), 15U);
        EXPECT_TRUE(output.contains("requestedCursor"));
        EXPECT_TRUE(output.contains("resultingCursor"));
        EXPECT_FALSE(output.contains("revision"));
    }
    EXPECT_EQ(writeRunJson(inspector.run()), writeRunJson(inspector.run()));
    EXPECT_EQ(writeStepJson(inspector.step(0U)), writeStepJson(inspector.step(0U)));
}

#if defined(G1_07_REPLAY_INSPECTOR_BINARY_PATH)
TEST(G107ReplayInspectorCli, RealProcessCoversAllCommandsAndIndependentOracle) {
    CliFixture fixture;
    const std::string trace = fixture.tracePath();
    const ReplayInspector oracle(makeMinimumTrace(), Provider::kReference);

    const std::vector<std::pair<std::vector<std::string>, const char*>> commands{
        {{"run", "--trace", trace, "--provider", "reference"}, "run"},
        {{"step", "--trace", trace, "--provider", "reference", "--index", "0"}, "step-boundary"},
        {{"seek", "--trace", trace, "--provider", "reference", "--position", "0"}, "seek"},
        {{"object", "--trace", trace, "--provider", "reference", "--position", "0",
          "--object-id", "000000000000000000000000000003e7"}, "object"},
        {{"projection", "--trace", trace, "--provider", "reference", "--position", "0"},
         "projection"},
    };

    for (const auto& [arguments, command] : commands) {
        const auto first = fixture.run(arguments);
        const auto repeat = fixture.run(arguments);
        EXPECT_EQ(first.exit_code, repeat.exit_code) << command;
        EXPECT_EQ(first.stdout_text, repeat.stdout_text) << command;
        EXPECT_EQ(first.stderr_text, repeat.stderr_text) << command;
        EXPECT_TRUE(first.stderr_text.empty()) << command;
        const auto output = parseOutput(first);
        expectEnvelope(output, std::string(command) == "step-boundary" ? "step" : command);
        if (std::string(command) == "step-boundary") {
            EXPECT_EQ(first.exit_code, 2);
            EXPECT_FALSE(output.at("success"));
            EXPECT_EQ(output.at("failure"), "POSITION_INVALID");
        } else {
            EXPECT_EQ(first.exit_code, 0);
            EXPECT_TRUE(output.at("success"));
            EXPECT_EQ(output.at("failure"), "NONE");
        }
    }

    const auto expected_run = parseOutput(fixture.run(
        {"run", "--trace", trace, "--provider", "reference"}));
    const auto expected_oracle = nlohmann::ordered_json::parse(writeRunJson(oracle.run()));
    EXPECT_EQ(expected_run.at("projection"), expected_oracle.at("projection"));
    EXPECT_EQ(expected_run.at("digestHex"), expected_oracle.at("digestHex"));
}

TEST(G107ReplayInspectorCli, ProcessErrorsAlwaysEmitJsonAndStableDiagnostics) {
    CliFixture fixture;
    const std::string trace = fixture.tracePath();
    const std::vector<std::pair<std::vector<std::string>, const char*>> usage_cases{
        {{}, "unknown"},
        {{"bogus"}, "bogus"},
        {{"run", "--trace", trace}, "run"},
        {{"run", "--trace"}, "run"},
        {{"run", "--trace", trace, "--provider"}, "run"},
        {{"run", "--trace", trace, "--provider", "bogus"}, "run"},
        {{"run", "--trace", trace, "--provider", "reference", "--unknown", "x"}, "run"},
        {{"run", "--trace", trace, "--trace", trace, "--provider", "reference"}, "run"},
        {{"step", "--trace", trace, "--provider", "reference"}, "step"},
        {{"step", "--trace", trace, "--provider", "reference", "--index", "x"}, "step"},
        {{"seek", "--trace", trace, "--provider", "reference", "--position", "-1"}, "seek"},
        {{"object", "--trace", trace, "--provider", "reference", "--position", "0",
          "--object-id", "not-an-object-id"}, "object"},
        {{"projection", "--trace", trace, "--provider", "reference", "--position", "0",
          "--index", "0"}, "projection"},
    };
    for (const auto& [arguments, command] : usage_cases) {
        const auto usage = fixture.run(arguments);
        const auto usage_repeat = fixture.run(arguments);
        EXPECT_EQ(usage.exit_code, 2) << command;
        EXPECT_EQ(usage.exit_code, usage_repeat.exit_code) << command;
        EXPECT_EQ(usage.stdout_text, usage_repeat.stdout_text) << command;
        EXPECT_EQ(usage.stderr_text, usage_repeat.stderr_text) << command;
        EXPECT_FALSE(usage.stderr_text.empty()) << command;
        const auto usage_json = parseOutput(usage);
        expectEnvelope(usage_json, command);
        EXPECT_FALSE(usage_json.at("success"));
        EXPECT_EQ(usage_json.at("failure"), "TRACE_INVALID");
    }

    const auto malformed_trace = std::filesystem::path(trace).parent_path() / "malformed.trace.json";
    {
        std::ofstream output(malformed_trace);
        output << "{}\n";
    }
    const auto structural = fixture.run(
        {"run", "--trace", malformed_trace.string(), "--provider", "reference"});
    EXPECT_EQ(structural.exit_code, 2);
    EXPECT_TRUE(structural.stderr_text.empty());
    const auto structural_json = parseOutput(structural);
    expectEnvelope(structural_json, "run");
    EXPECT_FALSE(structural_json.at("success"));
    EXPECT_EQ(structural_json.at("failure"), "TRACE_INVALID");

    const auto runtime = fixture.run(
        {"run", "--trace", (std::filesystem::path(trace).parent_path() / "missing.trace.json").string(),
         "--provider", "reference"});
    EXPECT_EQ(runtime.exit_code, 2);
    EXPECT_TRUE(runtime.stderr_text.empty());
    const auto runtime_json = parseOutput(runtime);
    expectEnvelope(runtime_json, "run");
    EXPECT_FALSE(runtime_json.at("success"));
    EXPECT_EQ(runtime_json.at("failure"), "TRACE_INVALID");

    const auto exit_three_trace = std::filesystem::path(trace).parent_path() / "snapshot-failure.trace.json";
    {
        std::ofstream output(exit_three_trace);
        output << R"({
  "format": "axiom-semantic-replay-trace-v1",
  "formatVersion": 1,
  "documentIdHex": "06070000000000000000000000000000",
  "schemaVersion": 1,
  "baseline": {
    "kind": "snapshot",
    "semanticGeneration": 0,
    "runtimeEpoch": 42,
    "commitOrdinal": 0,
    "snapshotBytesHex": "00"
  },
  "operations": []
})";
    }
    const auto semantic_failure = fixture.run(
        {"run", "--trace", exit_three_trace.string(), "--provider", "reference"});
    EXPECT_EQ(semantic_failure.exit_code, 3);
    EXPECT_TRUE(semantic_failure.stderr_text.empty());
    const auto semantic_failure_json = parseOutput(semantic_failure);
    expectEnvelope(semantic_failure_json, "run");
    EXPECT_FALSE(semantic_failure_json.at("success"));
    EXPECT_EQ(semantic_failure_json.at("failure"), "SNAPSHOT_DECODE_FAILED");
}
#endif

} // namespace canvas::verification::g1_07
