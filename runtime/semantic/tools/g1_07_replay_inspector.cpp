#include "g1_07_replay_inspector.hpp"

#include "canvas/semantic/normalizer.hpp"

#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/replay.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "object_store_mutator.hpp"
#include "g1_06_digest.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace canvas::verification::g1_07 {
namespace {

using Json = nlohmann::ordered_json;
using namespace canvas::semantic;

std::optional<std::vector<std::uint8_t>> parseHex(std::string_view value, std::size_t bytes) {
    if (value.size() != bytes * 2U || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) return std::nullopt;
    std::vector<std::uint8_t> result(bytes);
    for (std::size_t i = 0; i < bytes; ++i) {
        auto nibble = [](char c) -> std::uint8_t {
            return static_cast<std::uint8_t>(c <= '9' ? c - '0' : c - 'a' + 10);
        };
        result[i] = static_cast<std::uint8_t>((nibble(value[i * 2U]) << 4U) | nibble(value[i * 2U + 1U]));
    }
    return result;
}

std::optional<DocumentId> parseDocumentId(std::string_view value) {
    const auto bytes = parseHex(value, 16U);
    if (!bytes.has_value()) return std::nullopt;
    canvas::foundation::ObjectId raw;
    std::copy(bytes->begin(), bytes->end(), raw.bytes.begin());
    DocumentId result(raw);
    if (result.isZero()) return std::nullopt;
    return result;
}

std::optional<std::uint64_t> parseUnsigned(const Json& value) {
    if (!value.is_number_unsigned()) return std::nullopt;
    return value.get<std::uint64_t>();
}

TraceDecodeResult invalidTrace(std::string detail) {
    return {std::nullopt, FailureClass::kTraceInvalid, std::move(detail)};
}

FailureClass applyFailure(ApplyDisposition disposition) {
    switch (disposition) {
        case ApplyDisposition::kCommitBlocked: return FailureClass::kCommitBlocked;
        case ApplyDisposition::kRejected: return FailureClass::kApplyRejected;
        case ApplyDisposition::kApplied:
        case ApplyDisposition::kAlreadyApplied: return FailureClass::kNone;
    }
    return FailureClass::kApplyRejected;
}

std::string idHex(const foundation::ObjectId& value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : value.bytes) out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}

Json projectionValue(const std::string& value) {
    if (value.empty()) return nullptr;
    try {
        return Json::parse(value);
    } catch (...) {
        return value;
    }
}

Json writeChangeSet(const ChangeSet& value) {
    Json result = Json::object();
    result["beforeGeneration"] = value.beforeGeneration().value();
    result["afterGeneration"] = value.afterGeneration().value();
    result["objects"] = Json::array();
    for (const ObjectSemanticChange& change : value.objects()) {
        Json item = Json::object();
        item["objectIdHex"] = idHex(change.object_id);
        item["flags"] = static_cast<std::uint32_t>(change.flags);
        item["changedFields"] = Json::array();
        std::vector<FieldId> fields = change.changed_fields;
        std::sort(fields.begin(), fields.end());
        for (const FieldId field : fields) item["changedFields"].push_back(field);
        result["objects"].push_back(std::move(item));
    }
    return result;
}

Json writeCommitRecord(const CanonicalCommitRecord& value) {
    Json result = Json::object();
    result["operationIdHex"] = idHex(value.operation_id.value());
    result["applySource"] = value.source == ApplySource::kRestoreReplay ? "RESTORE_REPLAY" : "OTHER";
    result["beforeGeneration"] = value.before_generation.value();
    result["afterGeneration"] = value.after_generation.value();
    result["runtimeEpoch"] = value.commit_stamp.runtime_epoch.value();
    result["commitOrdinal"] = value.commit_stamp.ordinal.value();
    result["changeSet"] = writeChangeSet(value.change_set);
    return result;
}

std::string digestHex(std::uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << value;
    return out.str();
}

const char* runtimeStateName(DocumentRuntimeState value) noexcept {
    switch (value) {
        case DocumentRuntimeState::kConstructed: return "CONSTRUCTED";
        case DocumentRuntimeState::kLoading: return "LOADING";
        case DocumentRuntimeState::kReady: return "READY";
        case DocumentRuntimeState::kSuspended: return "SUSPENDED";
        case DocumentRuntimeState::kClosing: return "CLOSING";
        case DocumentRuntimeState::kClosed: return "CLOSED";
        case DocumentRuntimeState::kFailed: return "FAILED";
    }
    return "FAILED";
}

void addCommon(Json& result, std::string_view provider, std::string_view command,
               std::size_t operation_count, const std::optional<std::size_t>& requested,
               std::size_t resulting_position) {
    result["provider"] = provider;
    result["command"] = command;
    result["traceOperationCount"] = operation_count;
    if (requested.has_value()) result["requestedCursor"] = *requested;
    else result["requestedCursor"] = nullptr;
    result["resultingCursor"] = resulting_position;
}

template <typename Store>
ProjectionObservation summarize(const Store& objects, std::size_t position,
                                const DocumentId& document_id) {
    ProjectionObservation result;
    result.success = true;
    result.resulting_position = position;
    g1_06::ProjectionDocumentId projection_document_id{};
    std::copy(document_id.value().bytes.begin(), document_id.value().bytes.end(),
              projection_document_id.bytes.begin());
    const auto projection = projectDocument(projection_document_id, 1U, objects);
    result.projection_json = writeCanonicalProjectionJson(projection);
    result.digest = canvas::verification::g1_06::digestCanonicalProjectionBytes(result.projection_json);
    return result;
}

template <typename Store>
class Session final {
  public:
    explicit Session(const ReplayTrace& trace) : trace_(trace),
                                                   generation_(trace.baseline.generation),
                                                   clock_(trace.baseline.runtime_epoch,
                                                          trace.baseline.commit_ordinal) {}

    bool restore() {
        if (restored_) return true;
        if (trace_.baseline.kind == BaselineKind::kEmpty) {
            if (trace_.baseline.generation.value() != 0U || trace_.baseline.commit_ordinal.value() != 0U) {
                failure_ = FailureClass::kTraceInvalid;
                return false;
            }
            state_ = DocumentRuntimeState::kLoading;
            restored_ = true;
            return true;
        }
        if (!trace_.baseline.snapshot.has_value()) {
            failure_ = FailureClass::kSnapshotDecodeFailed;
            return false;
        }
        state_ = DocumentRuntimeState::kLoading;
        const auto result = SnapshotBootstrapper::restore(*trace_.baseline.snapshot, state_, objects_, generation_);
        if (!result.restored) {
            failure_ = FailureClass::kSnapshotBootstrapFailed;
            stateful_issue_ = result.semantic_error.issue;
            return false;
        }
        restored_ = true;
        return true;
    }

    bool applyPrefix(std::size_t count) {
        if (!restore()) return false;
        for (std::size_t index = 0U; index < count; ++index) {
            const ApplyResult result = engine_.apply(trace_.operations[index], ApplySource::kRestoreReplay,
                                                     objects_, ledger_, generation_, clock_);
            if (result.disposition != ApplyDisposition::kApplied &&
                result.disposition != ApplyDisposition::kAlreadyApplied) {
                failure_ = applyFailure(result.disposition);
                failure_index_ = index;
                failure_operation_id_ = trace_.operations[index].id;
                failure_disposition_ = result.disposition;
                stateful_issue_ = result.error.issue;
                commit_block_reason_ = result.commit_block_reason;
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] Store& objects() noexcept { return objects_; }
    [[nodiscard]] const Store& objects() const noexcept { return objects_; }
    [[nodiscard]] const std::optional<std::size_t>& failureIndex() const noexcept { return failure_index_; }
    [[nodiscard]] const std::optional<OperationId>& failureOperationId() const noexcept { return failure_operation_id_; }
    [[nodiscard]] ApplyDisposition failureDisposition() const noexcept { return failure_disposition_; }
    [[nodiscard]] FailureClass failure() const noexcept { return failure_; }
    [[nodiscard]] StatefulIssue statefulIssue() const noexcept { return stateful_issue_; }
    [[nodiscard]] CommitBlockReason commitBlockReason() const noexcept { return commit_block_reason_; }
    [[nodiscard]] SemanticGeneration generation() const noexcept { return generation_.current(); }
    [[nodiscard]] RuntimeEpoch runtimeEpoch() const noexcept { return clock_.runtimeEpoch(); }
    [[nodiscard]] CommitOrdinal commitOrdinal() const noexcept { return clock_.lastCommittedOrdinal(); }

    ApplyResult applyOne(std::size_t index) {
        return engine_.apply(trace_.operations[index], ApplySource::kRestoreReplay,
                             objects_, ledger_, generation_, clock_);
    }

    ReplayResult replayAll() {
        if (!restore()) return {};
        return ReplayCoordinator::replayAndFinalize(trace_.operations, state_, objects_, ledger_, generation_, clock_);
    }

    DocumentRuntimeState state() const noexcept { return state_; }

  private:
    const ReplayTrace& trace_;
    Store objects_;
    AppliedOperationLedger ledger_;
    SemanticGenerationState generation_;
    CanonicalCommitClock clock_;
    OperationEngine engine_;
    DocumentRuntimeState state_ = DocumentRuntimeState::kConstructed;
    bool restored_ = false;
    FailureClass failure_ = FailureClass::kNone;
    StatefulIssue stateful_issue_ = StatefulIssue::kNone;
    CommitBlockReason commit_block_reason_ = CommitBlockReason::kNone;
    std::optional<std::size_t> failure_index_;
    std::optional<OperationId> failure_operation_id_;
    ApplyDisposition failure_disposition_ = ApplyDisposition::kRejected;
};

template <typename Store>
StepObservation stepImpl(const ReplayTrace& trace, std::size_t index) {
    StepObservation result;
    result.operation_index = index;
    result.operation_id = trace.operations[index].id;
    Session<Store> session(trace);
    if (!session.applyPrefix(index)) {
        result.failure = session.failure();
        result.stateful_issue = session.statefulIssue();
        result.commit_block_reason = session.commitBlockReason();
        result.disposition = session.failureDisposition();
        result.resulting_position = session.failureIndex().value_or(0U);
        result.before_generation = session.generation();
        result.after_generation = session.generation();
        result.runtime_epoch = session.runtimeEpoch();
        result.commit_ordinal = session.commitOrdinal();
        return result;
    }
    const ApplyResult applied = session.applyOne(index);
    result.disposition = applied.disposition;
    result.stateful_issue = applied.error.issue;
    result.commit_block_reason = applied.commit_block_reason;
    result.commit_record = applied.commit_record;
    result.before_generation = applied.commit_record.has_value()
                                   ? applied.commit_record->before_generation
                                   : session.generation();
    result.after_generation = session.generation();
    result.runtime_epoch = session.runtimeEpoch();
    result.commit_ordinal = session.commitOrdinal();
    result.resulting_position = index + 1U;
    result.failure = applyFailure(applied.disposition);
    result.success = result.failure == FailureClass::kNone;
    if (!result.success) {
        result.resulting_position = index;
        result.stateful_issue = applied.error.issue;
    } else {
        const auto projection = summarize(session.objects(), result.resulting_position, trace.document_id);
        result.projection_json = projection.projection_json;
        result.digest = projection.digest;
    }
    return result;
}

template <typename Store>
ProjectionObservation seekImpl(const ReplayTrace& trace, std::size_t position) {
    ProjectionObservation result;
    if (position > trace.operations.size()) {
        result.failure = FailureClass::kPositionInvalid;
        return result;
    }
    Session<Store> session(trace);
    if (!session.applyPrefix(position)) {
        result.failure = session.failure();
        result.resulting_position = session.failureIndex().value_or(0U);
        result.semantic_generation = session.generation();
        result.runtime_epoch = session.runtimeEpoch();
        result.commit_ordinal = session.commitOrdinal();
        return result;
    }
    result = summarize(session.objects(), position, trace.document_id);
    result.semantic_generation = session.generation();
    result.runtime_epoch = session.runtimeEpoch();
    result.commit_ordinal = session.commitOrdinal();
    return result;
}

template <typename Store>
RunObservation runImpl(const ReplayTrace& trace) {
    RunObservation result;
    Session<Store> session(trace);
    const ReplayResult replay = session.replayAll();
    if (!replay.ready) {
        result.failure = session.failure();
        result.applied = replay.applied;
        result.already_applied = replay.already_applied;
        if (replay.failure.has_value()) {
            result.failure_index = replay.failure->operation_index;
            result.failure_operation_id = replay.failure->operation_id;
            result.failure_disposition = replay.failure->disposition;
            result.failure = applyFailure(replay.failure->disposition);
            result.resulting_position = replay.failure->operation_index;
        }
        result.state_after = session.state();
        result.semantic_generation = session.generation();
        result.runtime_epoch = session.runtimeEpoch();
        result.commit_ordinal = session.commitOrdinal();
        const auto projection = summarize(session.objects(), result.resulting_position, trace.document_id);
        result.projection_json = projection.projection_json;
        result.digest = projection.digest;
        return result;
    }
    result.success = true;
    result.applied = replay.applied;
    result.already_applied = replay.already_applied;
    result.resulting_position = trace.operations.size();
    result.state_after = session.state();
    result.semantic_generation = session.generation();
    result.runtime_epoch = session.runtimeEpoch();
    result.commit_ordinal = session.commitOrdinal();
    const auto projection = summarize(session.objects(), result.resulting_position, trace.document_id);
    result.projection_json = projection.projection_json;
    result.digest = projection.digest;
    return result;
}

template <typename Store>
ObjectObservation objectImpl(const ReplayTrace& trace, std::size_t position,
                             const foundation::ObjectId& object_id) {
    ObjectObservation result;
    if (position > trace.operations.size()) {
        result.failure = FailureClass::kPositionInvalid;
        return result;
    }
    Session<Store> session(trace);
    if (!session.applyPrefix(position)) {
        result.failure = session.failure();
        result.resulting_position = session.failureIndex().value_or(0U);
        result.semantic_generation = session.generation();
        result.runtime_epoch = session.runtimeEpoch();
        result.commit_ordinal = session.commitOrdinal();
        return result;
    }
    const auto projection = summarize(session.objects(), position, trace.document_id);
    result.success = true;
    result.resulting_position = position;
    result.semantic_generation = session.generation();
    result.runtime_epoch = session.runtimeEpoch();
    result.commit_ordinal = session.commitOrdinal();
    result.projection_json = projection.projection_json;
    result.digest = projection.digest;
    try {
        const ObjectRecord* canonical_record = session.objects().find(object_id);
        result.found = canonical_record != nullptr;
        const Json root = Json::parse(result.projection_json);
        const std::string wanted = [&] {
            std::ostringstream out;
            out << "id128:" << std::hex << std::setfill('0');
            for (const auto byte : object_id.bytes) out << std::setw(2) << static_cast<unsigned>(byte);
            return out.str();
        }();
        for (const auto& item : root.at("value").at("objects")) {
            if (item.at("id").get<std::string>() == wanted) {
                result.object_json = item.dump(2);
                break;
            }
        }
        if (result.found && result.object_json.empty()) throw std::runtime_error("projection missing object");
    } catch (...) {
        result.success = false;
        result.failure = FailureClass::kReplayFailed;
    }
    return result;
}

template <typename Store>
ProjectionObservation projectionImpl(const ReplayTrace& trace, std::size_t position) {
    return seekImpl<Store>(trace, position);
}

template <typename Store>
StepObservation dispatchStep(const ReplayTrace& trace, std::size_t index) { return stepImpl<Store>(trace, index); }

} // namespace

TraceDecodeResult decodeTraceJson(std::string_view json_text) {
    try {
        const Json root = Json::parse(json_text);
        if (!root.is_object() || root.value("format", "") != "axiom-semantic-replay-trace-v1" ||
            root.value("formatVersion", 0U) != 1U) return invalidTrace("format");
        const auto document = parseDocumentId(root.at("documentIdHex").get<std::string>());
        if (!document.has_value() || root.at("schemaVersion").get<std::uint32_t>() != 1U) return invalidTrace("identity");
        const auto& baseline = root.at("baseline");
        ReplayTrace trace;
        trace.document_id = *document;
        trace.schema_version = 1U;
        const std::string kind = baseline.at("kind").get<std::string>();
        if (kind == "empty") trace.baseline.kind = BaselineKind::kEmpty;
        else if (kind == "snapshot") trace.baseline.kind = BaselineKind::kSnapshot;
        else return invalidTrace("baseline kind");
        const auto generation = parseUnsigned(baseline.at("semanticGeneration"));
        const auto epoch = parseUnsigned(baseline.at("runtimeEpoch"));
        const auto ordinal = parseUnsigned(baseline.at("commitOrdinal"));
        if (!generation.has_value() || !epoch.has_value() || !ordinal.has_value()) return invalidTrace("baseline identity");
        trace.baseline.generation = SemanticGeneration(*generation);
        trace.baseline.runtime_epoch = RuntimeEpoch(*epoch);
        trace.baseline.commit_ordinal = CommitOrdinal(*ordinal);
        if (trace.baseline.kind == BaselineKind::kSnapshot) {
            const auto bytes = parseHex(baseline.at("snapshotBytesHex").get<std::string>(),
                                        baseline.at("snapshotBytesHex").get<std::string>().size() / 2U);
            if (!bytes.has_value()) return invalidTrace("snapshot hex");
            const auto decoded = SnapshotCodec::decode(*bytes);
            if (!decoded.ok()) return {std::nullopt, FailureClass::kSnapshotDecodeFailed, "snapshot"};
            trace.baseline.snapshot = *decoded.snapshot;
            if (trace.baseline.snapshot->document_id != trace.document_id) return invalidTrace("snapshot document");
        } else if (trace.baseline.generation.value() != 0U || trace.baseline.commit_ordinal.value() != 0U) {
            return invalidTrace("empty baseline");
        }
        if (!root.at("operations").is_array()) return invalidTrace("operations");
        for (const auto& item : root.at("operations")) {
            const std::string bytes_text = item.at("bytesHex").get<std::string>();
            const auto bytes = parseHex(bytes_text, bytes_text.size() / 2U);
            if (!bytes.has_value()) return invalidTrace("operation hex");
            const auto decoded = SemanticCodec::decodeProtobufOperation(*bytes);
            if (!decoded.ok()) return {std::nullopt, FailureClass::kTraceInvalid, "operation decode"};
            const auto normalized = normalizeOperation(decoded.operation);
            if (!normalized.ok()) return {std::nullopt, FailureClass::kTraceInvalid, "operation normalize"};
            if (normalized.value.document_id != trace.document_id || normalized.value.schema_version != 1U) {
                return invalidTrace("operation identity");
            }
            trace.operations.push_back(normalized.value);
        }
        return {std::move(trace), FailureClass::kNone, {}};
    } catch (const std::exception& error) {
        return invalidTrace(error.what());
    }
}

TraceDecodeResult readTraceFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) return invalidTrace("unable to open trace");
    return decodeTraceJson(std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()));
}

const char* failureClassName(FailureClass value) noexcept {
    switch (value) {
        case FailureClass::kNone: return "NONE";
        case FailureClass::kTraceInvalid: return "TRACE_INVALID";
        case FailureClass::kSnapshotDecodeFailed: return "SNAPSHOT_DECODE_FAILED";
        case FailureClass::kSnapshotBootstrapFailed: return "SNAPSHOT_BOOTSTRAP_FAILED";
        case FailureClass::kPositionInvalid: return "POSITION_INVALID";
        case FailureClass::kApplyRejected: return "APPLY_REJECTED";
        case FailureClass::kCommitBlocked: return "COMMIT_BLOCKED";
        case FailureClass::kReplayFailed: return "REPLAY_FAILED";
    }
    return "REPLAY_FAILED";
}

const char* dispositionName(ApplyDisposition value) noexcept {
    switch (value) {
        case ApplyDisposition::kApplied: return "APPLIED";
        case ApplyDisposition::kAlreadyApplied: return "ALREADY_APPLIED";
        case ApplyDisposition::kRejected: return "REJECTED";
        case ApplyDisposition::kCommitBlocked: return "COMMIT_BLOCKED";
    }
    return "REJECTED";
}

const char* statefulIssueName(StatefulIssue value) noexcept {
    switch (value) {
        case StatefulIssue::kNone: return "NONE";
        case StatefulIssue::kObjectMissing: return "OBJECT_MISSING";
        case StatefulIssue::kObjectAlreadyExists: return "OBJECT_ALREADY_EXISTS";
        case StatefulIssue::kInvalidKindVersion: return "INVALID_KIND_VERSION";
        case StatefulIssue::kInvalidApplicability: return "INVALID_APPLICABILITY";
        case StatefulIssue::kInvalidReference: return "INVALID_REFERENCE";
        case StatefulIssue::kHierarchyCycle: return "HIERARCHY_CYCLE";
        case StatefulIssue::kConnectorInvalid: return "CONNECTOR_INVALID";
        case StatefulIssue::kMaskStateInvalid: return "MASK_STATE_INVALID";
        case StatefulIssue::kTextStateInvalid: return "TEXT_STATE_INVALID";
        case StatefulIssue::kOperationIdCollision: return "OPERATION_ID_COLLISION";
    }
    return "NONE";
}

const char* commitBlockReasonName(CommitBlockReason value) noexcept {
    switch (value) {
        case CommitBlockReason::kNone: return "NONE";
        case CommitBlockReason::kInvalidRuntimeEpoch: return "INVALID_RUNTIME_EPOCH";
        case CommitBlockReason::kCommitLaneExhausted: return "COMMIT_LANE_EXHAUSTED";
    }
    return "NONE";
}

Json outputEnvelope() {
    Json result = Json::object();
    result["format"] = "axiom-semantic-replay-inspector-output-v1";
    result["formatVersion"] = 1U;
    return result;
}

std::string writeStepJson(const StepObservation& value) {
    Json result = outputEnvelope();
    addCommon(result, value.provider, value.command, value.trace_operation_count,
              value.requested_index, value.resulting_position);
    result["success"] = value.success;
    result["failure"] = failureClassName(value.failure);
    result["operationIndex"] = value.operation_index;
    result["operationIdHex"] = idHex(value.operation_id.value());
    result["resultingPosition"] = value.resulting_position;
    result["disposition"] = dispositionName(value.disposition);
    result["statefulIssue"] = statefulIssueName(value.stateful_issue);
    result["commitBlockReason"] = commitBlockReasonName(value.commit_block_reason);
    result["beforeGeneration"] = value.before_generation.value();
    result["afterGeneration"] = value.after_generation.value();
    result["runtimeEpoch"] = value.runtime_epoch.value();
    result["commitOrdinal"] = value.commit_ordinal.value();
    if (value.commit_record.has_value()) result["commitRecord"] = writeCommitRecord(*value.commit_record);
    else result["commitRecord"] = nullptr;
    result["projection"] = projectionValue(value.projection_json);
    result["digestHex"] = digestHex(value.digest);
    return result.dump(2) + "\n";
}
std::string writeRunJson(const RunObservation& value) {
    Json result = outputEnvelope();
    addCommon(result, value.provider, value.command, value.trace_operation_count,
              value.requested_position, value.resulting_position);
    result["success"] = value.success;
    result["failure"] = failureClassName(value.failure);
    result["applied"] = value.applied;
    result["alreadyApplied"] = value.already_applied;
    if (value.failure_operation_id.has_value()) {
        result["failureOperationIdHex"] = idHex(value.failure_operation_id->value());
    }
    if (value.failure_index.has_value()) result["failureIndex"] = *value.failure_index;
    result["failureDisposition"] = dispositionName(value.failure_disposition);
    result["stateAfter"] = runtimeStateName(value.state_after);
    result["semanticGeneration"] = value.semantic_generation.value();
    result["runtimeEpoch"] = value.runtime_epoch.value();
    result["commitOrdinal"] = value.commit_ordinal.value();
    result["projection"] = projectionValue(value.projection_json);
    result["digestHex"] = digestHex(value.digest);
    return result.dump(2) + "\n";
}
std::string writeObjectJson(const ObjectObservation& value) {
    Json result = outputEnvelope();
    addCommon(result, value.provider, value.command, value.trace_operation_count,
              value.requested_position, value.resulting_position);
    result["success"] = value.success;
    result["failure"] = failureClassName(value.failure);
    result["found"] = value.found;
    result["semanticGeneration"] = value.semantic_generation.value();
    result["runtimeEpoch"] = value.runtime_epoch.value();
    result["commitOrdinal"] = value.commit_ordinal.value();
    result["objectIdHex"] = value.object_id_hex;
    result["object"] = projectionValue(value.object_json);
    result["projection"] = projectionValue(value.projection_json);
    result["digestHex"] = digestHex(value.digest);
    return result.dump(2) + "\n";
}
std::string writeProjectionJson(const ProjectionObservation& value) {
    Json result = outputEnvelope();
    addCommon(result, value.provider, value.command, value.trace_operation_count,
              value.requested_position, value.resulting_position);
    result["success"] = value.success;
    result["failure"] = failureClassName(value.failure);
    result["semanticGeneration"] = value.semantic_generation.value();
    result["runtimeEpoch"] = value.runtime_epoch.value();
    result["commitOrdinal"] = value.commit_ordinal.value();
    result["projection"] = projectionValue(value.projection_json);
    result["digestHex"] = digestHex(value.digest);
    return result.dump(2) + "\n";
}

ReplayInspector::ReplayInspector(ReplayTrace trace, Provider provider) : trace_(std::move(trace)), provider_(provider) {}

namespace {
const char* providerName(Provider value) noexcept {
    return value == Provider::kReference ? "reference" : "indexed";
}
template <typename Observation>
void annotate(Observation& value, Provider provider, std::string_view command,
              std::size_t count, std::optional<std::size_t> requested) {
    value.provider = providerName(provider);
    value.command = std::string(command);
    value.trace_operation_count = count;
    value.requested_position = requested;
}
} // namespace

StepObservation ReplayInspector::step(std::size_t index) const {
    StepObservation result;
    if (index < trace_.operations.size()) {
        result = provider_ == Provider::kReference ? dispatchStep<ReferenceObjectStore>(trace_, index) : dispatchStep<IndexedObjectStore>(trace_, index);
    } else {
        result.failure = FailureClass::kPositionInvalid;
        result.operation_index = index;
    }
    result.provider = providerName(provider_);
    result.command = "step";
    result.trace_operation_count = trace_.operations.size();
    result.requested_index = index;
    return result;
}
ProjectionObservation ReplayInspector::seek(std::size_t position) const {
    ProjectionObservation result = provider_ == Provider::kReference ? seekImpl<ReferenceObjectStore>(trace_, position) : seekImpl<IndexedObjectStore>(trace_, position);
    annotate(result, provider_, "seek", trace_.operations.size(), position);
    return result;
}
RunObservation ReplayInspector::run() const {
    RunObservation result = provider_ == Provider::kReference ? runImpl<ReferenceObjectStore>(trace_) : runImpl<IndexedObjectStore>(trace_);
    result.provider = providerName(provider_);
    result.command = "run";
    result.trace_operation_count = trace_.operations.size();
    result.requested_position = trace_.operations.size();
    return result;
}
ObjectObservation ReplayInspector::object(std::size_t position, const foundation::ObjectId& object_id) const {
    ObjectObservation result = provider_ == Provider::kReference ? objectImpl<ReferenceObjectStore>(trace_, position, object_id) : objectImpl<IndexedObjectStore>(trace_, position, object_id);
    result.provider = providerName(provider_);
    result.command = "object";
    result.trace_operation_count = trace_.operations.size();
    result.requested_position = position;
    result.object_id_hex = idHex(object_id);
    return result;
}
ProjectionObservation ReplayInspector::projection(std::size_t position) const {
    ProjectionObservation result = seek(position);
    result.command = "projection";
    return result;
}

} // namespace canvas::verification::g1_07
