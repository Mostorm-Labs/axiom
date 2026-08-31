#pragma once

#include "g1_04_c_fixture_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace canvas::verification::g1_04_c {

enum class Provider { kReference, kIndexed };

struct ManifestInventory final {
    bool ok = false;
    std::string error;
    std::vector<std::string> caseIds;
    std::vector<std::string> inputPaths;
};

struct CorpusReport final {
    bool ok = false;
    std::string error;
    std::size_t acceptedCases = 0;
    std::size_t observations = 0;
    std::size_t unexpectedHarnessErrors = 0;
    std::size_t unchangedObservations = 0;
    std::size_t expectedTruthReads = 0;
    std::size_t semanticCodecCalls = 0;
    std::vector<std::string> observedCaseIds;

    struct ObservationFact final {
        std::string caseId;
        std::string provider;
        std::string disposition;
        std::string terminalPhase;
        std::string issue;
        std::string planProjection;
        std::string beforeProjection;
        std::string afterProjection;
        std::vector<std::uint64_t> f64Bits;
    };
    std::vector<ObservationFact> observationFacts;

    [[nodiscard]] bool caseObserved(const std::string& id) const;
};

[[nodiscard]] ManifestInventory loadAcceptedManifest(const std::string& path);
[[nodiscard]] CorpusReport runAcceptedCorpus(const std::string& manifestPath);
[[nodiscard]] std::vector<canvas::semantic::ObjectRecord> makeSyntheticStoreObjects();

} // namespace canvas::verification::g1_04_c
