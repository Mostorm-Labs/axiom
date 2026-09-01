#include "g1_04_c_fixture_runtime.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

namespace canvas::verification::g1_04_c {
namespace {
using json = nlohmann::json;

json objectProjectionJson(const ObjectProjection& projection) {
    json objects = json::array();
    for (const auto& object : projection.objects) {
        json value{
            {"id", object.id},
            {"kind", object.kind},
            {"kindVersion", object.kindVersion},
            {"orderKeyHex", object.orderKeyHex},
        };
        if (object.parentId.has_value()) value["parentId"] = *object.parentId;
        objects.push_back(std::move(value));
    }
    return json{{"objects", std::move(objects)}};
}

json planProjectionJson(const PlanProjection& projection) {
    json value{
        {"creates", objectProjectionJson(projection.creates)},
        {"replacements", objectProjectionJson(projection.replacements)},
        {"deletes", projection.deletes},
    };
    if (projection.deleteClosure.has_value()) value["deleteClosure"] = *projection.deleteClosure;
    return value;
}

json observationJson(const CorpusReport::ObservationFact& fact) {
    json value{
        {"format", "axiom-g1-04-c-observation-v1"},
        {"formatVersion", 1},
        {"provenance", "IMPLEMENTATION_OBSERVATION"},
        {"caseId", fact.caseId},
        {"provider", fact.provider},
        {"observedDisposition", fact.disposition},
        {"observedTerminalPhase", fact.terminalPhase},
        {"beforeProjection", objectProjectionJson(fact.beforeProjection)},
        {"afterProjection", objectProjectionJson(fact.afterProjection)},
    };
    if (!fact.issue.empty()) value["observedErrorCategory"] = fact.issue;
    if (fact.observedPlanProjection.has_value()) {
        value["observedPlanProjection"] = planProjectionJson(*fact.observedPlanProjection);
    }
    return value;
}

std::string f64Hex(const std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

json negativeZeroAudit(const CorpusReport& report) {
    json byProvider = json::object();
    for (const auto& fact : report.observationFacts) {
        if (fact.caseId != "C1-TRANSFORM-NEGATIVE-ZERO") continue;
        json bits = json::array();
        for (const auto value : fact.f64Bits) bits.push_back(f64Hex(value));
        byProvider[fact.provider] = std::move(bits);
    }
    return json{
        {"caseId", "C1-TRANSFORM-NEGATIVE-ZERO"},
        {"requiredBits", "8000000000000000"},
        {"byProvider", std::move(byProvider)},
    };
}
} // namespace
} // namespace canvas::verification::g1_04_c

int main(int argc, char** argv) {
    using namespace canvas::verification::g1_04_c;
    if (argc != 2) {
        std::cerr << "usage: canvas_semantic_g1_04_c_observation_export <manifest-path>\n";
        return 2;
    }
    const auto report = runAcceptedCorpus(argv[1]);
    if (!report.ok || report.acceptedCases != 90U || report.observations != 180U ||
        report.unchangedObservations != 180U || report.unexpectedHarnessErrors != 0U) {
        std::cerr << "unable to observe accepted C3 corpus: " << report.error << '\n';
        return 1;
    }
    nlohmann::json observations = nlohmann::json::array();
    for (const auto& fact : report.observationFacts) observations.push_back(observationJson(fact));
    const nlohmann::json output{
        {"format", "axiom-gt-g1-04-c-plan-projection-v2"},
        {"formatVersion", 2},
        {"factsOnly", true},
        {"acceptedCases", report.acceptedCases},
        {"observationCount", report.observations},
        {"noMutationObservations", report.unchangedObservations},
        {"unexpectedHarnessErrors", report.unexpectedHarnessErrors},
        {"providers", {"reference", "indexed"}},
        {"expectedTruthReads", report.expectedTruthReads},
        {"semanticCodecCalls", report.semanticCodecCalls},
        {"decodedInputAudit", {{"negativeZero", negativeZeroAudit(report)}}},
        {"observationRecords", std::move(observations)},
    };
    std::cout << output.dump(2) << '\n';
    return 0;
}
