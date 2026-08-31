#include "g1_04_c_fixture_runtime.hpp"

#include "g1_04_c_projection.hpp"
#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/validator.hpp"
#include "object_store_mutator.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <map>
#include <memory>

namespace canvas::verification::g1_04_c {
namespace {
using json = nlohmann::json;
using namespace canvas::semantic;

class FixtureAppliedOperations final : public AppliedOperationView {
  public:
    explicit FixtureAppliedOperations(const std::vector<Operation>& operations) : operations_(operations) {}
    std::optional<AppliedOperationEntry> find(const OperationId& id) const override {
        for (const auto& operation : operations_) {
            if (operation.id == id) return AppliedOperationEntry{operation, std::nullopt};
        }
        return std::nullopt;
    }
  private:
    const std::vector<Operation>& operations_;
};

template <typename Store>
bool bootstrap(Store& store, const DecodedFixture& fixture) {
    for (const auto& record : fixture.initialObjects) {
        if (!internal::ObjectStoreMutator::insertFresh(store, record)) return false;
    }
    return true;
}

template <typename Store>
CorpusReport::ObservationFact observeOne(Store& store, const DecodedFixture& fixture, const char* provider) {
    const std::string before = projectStore(store);
    FixtureAppliedOperations applied(fixture.priorOperations);
    const StatefulValidationContext context{store, applied};
    Operation candidate = fixture.operation;
    CorpusReport::ObservationFact fact;
    fact.caseId = fixture.caseId;
    fact.provider = provider;
    fact.beforeProjection = before;
    fact.f64Bits = fixture.f64Bits;
    const auto normalized = normalizeOperation(candidate);
    if (!normalized.ok()) {
        fact.disposition = "REJECTED"; fact.terminalPhase = "STATELESS_VALIDATE"; fact.issue = "NORMALIZE_ERROR";
    } else {
        candidate = normalized.value;
        const OperationFieldPresence presence{true, true};
        const auto envelope = validateEnvelope(candidate, presence);
        if (!envelope.ok()) {
            fact.disposition = "REJECTED"; fact.terminalPhase = "STATELESS_VALIDATE"; fact.issue = "ENVELOPE_ERROR";
        } else {
            const auto payload = validatePayloadStructure(candidate);
            if (!payload.ok()) {
                fact.disposition = "REJECTED"; fact.terminalPhase = "STATELESS_VALIDATE"; fact.issue = "PAYLOAD_ERROR";
            } else {
                const auto prepared = prepareApplyPlan(candidate, context);
                if (prepared.disposition == PrepareDisposition::kPrepared) {
                    fact.disposition = "PLAN_READY"; fact.terminalPhase = "PREPARE";
                } else if (prepared.disposition == PrepareDisposition::kAlreadyApplied) {
                    fact.disposition = "ALREADY_APPLIED"; fact.terminalPhase = "PREPARE";
                } else {
                    fact.disposition = "REJECTED"; fact.terminalPhase = "STATEFUL_VALIDATE";
                    fact.issue = std::to_string(static_cast<unsigned>(prepared.error.issue));
                }
            }
        }
    }
    const std::string after = projectStore(store);
    fact.afterProjection = after;
    return fact;
}

} // namespace

bool CorpusReport::caseObserved(const std::string& id) const {
    return std::find(observedCaseIds.begin(), observedCaseIds.end(), id) != observedCaseIds.end();
}

ManifestInventory loadAcceptedManifest(const std::string& path) {
    ManifestInventory inventory;
    try {
        std::ifstream input(path); if (!input) { inventory.error = "unable to open manifest: " + path; return inventory; }
        const auto manifest = json::parse(input);
        for (const auto& entry : manifest.at("entries")) {
            inventory.caseIds.push_back(entry.at("caseId").get<std::string>());
            inventory.inputPaths.push_back(entry.at("input").at("path").get<std::string>());
        }
        inventory.ok = manifest.value("caseCount", 0U) == inventory.caseIds.size();
        if (!inventory.ok) inventory.error = "manifest case count mismatch";
    } catch (const std::exception& e) { inventory.error = e.what(); }
    return inventory;
}

CorpusReport runAcceptedCorpus(const std::string& manifestPath) {
    CorpusReport report;
    const auto inventory = loadAcceptedManifest(manifestPath);
    if (!inventory.ok) { report.error = inventory.error; return report; }
    const std::filesystem::path manifest = manifestPath;
    const auto root = manifest.parent_path();
    report.acceptedCases = inventory.caseIds.size();
    for (std::size_t i = 0; i < inventory.caseIds.size(); ++i) {
        const auto decoded = decodeFixtureFile((root / inventory.inputPaths[i]).string());
        if (!decoded.ok || !decoded.fixture.has_value()) { ++report.unexpectedHarnessErrors; report.error += " decode:" + inventory.caseIds[i] + "=" + decoded.error; continue; }
        report.observedCaseIds.push_back(decoded.fixture->caseId);
        for (const Provider provider : {Provider::kReference, Provider::kIndexed}) {
            bool unchanged = false;
            if (provider == Provider::kReference) {
                ReferenceObjectStore store;
                if (!bootstrap(store, *decoded.fixture)) { ++report.unexpectedHarnessErrors; report.error += " bootstrap-reference:" + decoded.fixture->caseId; continue; }
                auto fact = observeOne(store, *decoded.fixture, "reference"); unchanged = fact.beforeProjection == fact.afterProjection; report.observationFacts.push_back(std::move(fact));
            } else {
                IndexedObjectStore store;
                if (!bootstrap(store, *decoded.fixture)) { ++report.unexpectedHarnessErrors; report.error += " bootstrap-indexed:" + decoded.fixture->caseId; continue; }
                auto fact = observeOne(store, *decoded.fixture, "indexed"); unchanged = fact.beforeProjection == fact.afterProjection; report.observationFacts.push_back(std::move(fact));
            }
            ++report.observations;
            if (unchanged) ++report.unchangedObservations;
        }
    }
    report.ok = report.unexpectedHarnessErrors == 0U && report.acceptedCases == 90U && report.observations == 180U;
    if (!report.ok && report.error.empty()) report.error = "accepted corpus observation thresholds not met";
    return report;
}

std::vector<canvas::semantic::ObjectRecord> makeSyntheticStoreObjects() {
    canvas::semantic::ObjectRecord object;
    object.id = canvas::foundation::ObjectId::fromUint64(1U);
    object.kind = canvas::semantic::ObjectKind::kShape;
    object.kind_version = 1U;
    object.placement.order_key = canvas::semantic::OrderKey({1U});
    object.content = canvas::semantic::ShapeContent{1U, 10.0, 10.0};
    return {object};
}

} // namespace canvas::verification::g1_04_c
