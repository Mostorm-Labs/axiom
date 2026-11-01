#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/section_content.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "canvas/semantic/validator.hpp"
#include "auditoryworks/axiom/v1/operation.pb.h"
#include "auditoryworks/axiom/v1/snapshot.pb.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace canvas::semantic;
namespace p = auditoryworks::axiom::v1;
using Json = nlohmann::json;
const std::filesystem::path kRoot(AXIOM_SOURCE_ROOT);

Json readJson(const std::filesystem::path& path) {
    std::ifstream stream(path);
    return Json::parse(stream);
}
std::vector<std::uint8_t> readWire(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(stream)), {}};
}
std::string hex(const std::string& bytes) {
    std::string result;
    for (const unsigned char b : bytes) {
        result += "0123456789abcdef"[b >> 4U];
        result += "0123456789abcdef"[b & 15U];
    }
    return result;
}
std::vector<std::uint8_t> wire(const std::string& bytes) { return {bytes.begin(), bytes.end()}; }
const Json& corpus() {
    static const Json value = readJson(kRoot / ".aegis/materialized/FC-08/attempts/v0.3-control-remediation-02/corpus-manifest.json");
    return value;
}
std::vector<Json> contentCases() {
    std::vector<Json> cases;
    for (const auto& row : corpus()["variants"])
        if (row["case_id"] == "FC08-W02") cases.push_back(row);
    return cases;
}
ObjectRecord section() {
    ObjectRecord object{};
    object.id = ObjectId::fromUint64(1);
    object.kind = ObjectKind::kSection;
    object.kind_version = 1;
    object.placement.order_key = OrderKey({0x10});
    object.content = SectionContent{{-20.0, 0.0, 120.0, 80.0}, "", 8.0, 20.0};
    return object;
}
Operation operation(OperationPayload payload, std::uint64_t id = 2) {
    return {OperationId{ObjectId::fromUint64(id)}, DocumentId{ObjectId::fromUint64(3)},
            1, 1, std::move(payload)};
}
void appendVarint(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    do {
        auto b = static_cast<std::uint8_t>(value & 0x7fU);
        value >>= 7U;
        if (value) b |= 0x80U;
        bytes.push_back(b);
    } while (value);
}
std::vector<std::uint8_t> field(std::uint32_t tag, const std::vector<std::uint8_t>& content) {
    std::vector<std::uint8_t> result;
    appendVarint(result, (static_cast<std::uint64_t>(tag) << 3U) | 2U);
    appendVarint(result, content.size());
    result.insert(result.end(), content.begin(), content.end());
    return result;
}
// Only the valid envelope/carrier is encoded by protobuf. The frozen leaf bytes
// are inserted verbatim, preserving duplicate/unknown fields and invalid UTF8.
std::vector<std::uint8_t> wrapLeaf(const std::vector<std::uint8_t>& leaf) {
    p::ObjectRecord record;
    const auto reference = readWire(kRoot / "tests/fc08/corpus/wire/section-record.pb");
    if (!record.ParseFromArray(reference.data(), static_cast<int>(reference.size()))) return {};
    record.clear_content();
    auto record_bytes = wire(record.SerializeAsString());
    const auto content = field(7, field(12, leaf));
    record_bytes.insert(record_bytes.end(), content.begin(), content.end());
    p::Operation envelope;
    const auto base = SemanticCodec::encodeProtobufOperation(operation(InsertObjectsOp{{section()}}));
    if (!base.ok() || !envelope.ParseFromArray(base.bytes.data(), static_cast<int>(base.bytes.size()))) return {};
    envelope.clear_payload();
    auto bytes = wire(envelope.SerializeAsString());
    const auto payload = field(5, field(1, field(1, record_bytes)));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

class Fc08SectionContent : public ::testing::TestWithParam<Json> {};
TEST_P(Fc08SectionContent, FrozenContractThroughRealCodecAndIngress) {
    const auto& row = GetParam();
    SCOPED_TRACE(row["variant_id"].get<std::string>());
    const auto input = readJson(kRoot / row["input"]["path"].get<std::string>());
    const auto expected = readJson(kRoot / row["expected"]["path"].get<std::string>());
    std::vector<std::uint8_t> bytes;
    std::string canonical;
    bool accepted = false;
    if (input["direction"] == "model-to-canonical-writer") {
        auto object = section();
        const auto& model = input["wire_model"];
        const auto& b = model["boundary"];
        object.content = SectionContent{{b["x"].get<double>(), b["y"].get<double>(),
            b["width"].get<double>(), b["height"].get<double>()},
            model["title"].get<std::string>(), model["padding"].get<double>(), model["header_height"].get<double>()};
        const auto normalized = normalizeOperation(operation(InsertObjectsOp{{object}}));
        ASSERT_TRUE(normalized.ok());
        ASSERT_TRUE(validatePayloadStructure(normalized.value).ok());
        const auto encoded = SemanticCodec::encodeProtobufOperation(normalized.value);
        ASSERT_TRUE(encoded.ok());
        bytes = encoded.bytes;
        p::Operation actual;
        ASSERT_TRUE(actual.ParseFromArray(bytes.data(), static_cast<int>(bytes.size())));
        canonical = actual.payload().insert_objects().objects(0).content().section().SerializeAsString();
        accepted = true;
    } else {
        const auto leaf = readWire(kRoot / row["wire"][0]["path"].get<std::string>());
        ASSERT_FALSE(leaf.empty());
        const auto observed = SemanticCodec::observeGoldenFixture("SectionContent", leaf, true);
        accepted = observed.accepted;
        canonical.assign(observed.canonical_bytes.begin(), observed.canonical_bytes.end());
        bytes = wrapLeaf(leaf);
    }
    ASSERT_FALSE(bytes.empty());
    const auto decoded = SemanticCodec::decodeProtobufOperation(bytes);
    ReferenceObjectStore objects;
    AppliedOperationLedger ledger;
    SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(17));
    bool admitted = false;
    if (decoded.ok()) {
        const auto result = OperationEngine{}.apply(decoded.operation, ApplySource::kLocalInteraction,
                                                   objects, ledger, generation, clock);
        admitted = result.disposition == ApplyDisposition::kApplied;
    }
    EXPECT_EQ(admitted, accepted);
    for (const auto& predicate : expected["predicates"]) {
        if (predicate["op"] == "unchanged") {
            ASSERT_EQ(input["before"], Json::array());
            EXPECT_TRUE(objects.allObjects().empty());
            EXPECT_EQ(generation.current().value(), 0U);
            EXPECT_EQ(clock.lastCommittedOrdinal().value(), 0U);
        } else if (predicate["op"] == "equal" && predicate["path"] == "accepted") {
            EXPECT_EQ(accepted, predicate["expected"].get<bool>());
        } else if (predicate["op"] == "equal" && predicate["path"] == "canonical_hex") {
            EXPECT_EQ(hex(canonical), predicate["expected"].get<std::string>());
        } else if (predicate["op"] == "equal" && predicate["path"] == "decoded.title_utf8") {
            ASSERT_TRUE(decoded.ok());
            const auto& content = std::get<SectionContent>(std::get<InsertObjectsOp>(decoded.operation.payload).objects[0].content);
            EXPECT_EQ(hex(content.title), predicate["expected"].get<std::string>());
        } else {
            FAIL() << "Unimplemented frozen predicate: " << predicate.dump();
        }
    }
}
INSTANTIATE_TEST_SUITE_P(FrozenW02, Fc08SectionContent, ::testing::ValuesIn(contentCases()),
    [](const testing::TestParamInfo<Json>& info) {
        auto name = info.param["variant_id"].get<std::string>().substr(9);
        for (auto& c : name) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) c = '_';
        return name;
    });

TEST(Fc08SectionA, IndependentlyAuthoredSectionRecordAccepted) {
    const auto bytes = readWire(kRoot / "tests/fc08/corpus/wire/section-record.pb");
    const auto result = SemanticCodec::observeGoldenFixture("ObjectRecord", bytes, true);
    ASSERT_TRUE(result.accepted) << result.category;
    EXPECT_EQ(result.canonical_bytes, bytes);
}
TEST(Fc08SectionA, RejectsNonExactWireKindBeforeNarrowing) {
    p::ObjectRecord record;
    const auto bytes = readWire(kRoot / "tests/fc08/corpus/wire/section-record.pb");
    ASSERT_TRUE(record.ParseFromArray(bytes.data(), static_cast<int>(bytes.size())));
    for (const auto kind : {266U, 10U + 65536U, 0xffffffffU}) {
        record.set_kind_id(kind);
        EXPECT_FALSE(SemanticCodec::observeGoldenFixture("ObjectRecord", wire(record.SerializeAsString()), false).accepted);
    }
}
TEST(Fc08SectionA, RejectsWrongVersionContentAndMasks) {
    auto object = section();
    object.kind_version = 2;
    EXPECT_FALSE(validatePayloadStructure(operation(InsertObjectsOp{{object}})).ok());
    object = section(); object.content = GroupContent{};
    EXPECT_FALSE(validatePayloadStructure(operation(InsertObjectsOp{{object}})).ok());
    object = section(); object.erase_masks.push_back({ObjectId::fromUint64(8), SweptCircleMask{}});
    EXPECT_FALSE(validatePayloadStructure(operation(InsertObjectsOp{{object}})).ok());
}
TEST(Fc08SectionA, ModelDomainRejectsEndpointOverflowAndOverBudgetTitle) {
    auto object = section();
    auto& content = std::get<SectionContent>(object.content);
    content.boundary.x = std::numeric_limits<double>::max();
    content.boundary.width = std::numeric_limits<double>::max();
    EXPECT_FALSE(validatePayloadStructure(operation(InsertObjectsOp{{object}})).ok());
    object = section();
    std::get<SectionContent>(object.content).title.assign(1024U * 1024U + 1U, 'a');
    EXPECT_FALSE(validatePayloadStructure(operation(InsertObjectsOp{{object}})).ok());
}
TEST(Fc08SectionA, ExistingInsertOwnerAcceptsSectionParentAndPreservesChildTransform) {
    auto child = section();
    child.id = ObjectId::fromUint64(4);
    child.kind = ObjectKind::kShape;
    child.content = ShapeContent{1, 20, 20};
    child.placement.parent_id = section().id;
    child.transform.tx = 43; child.transform.ty = -27;
    ReferenceObjectStore objects; AppliedOperationLedger ledger;
    SemanticGenerationState generation; CanonicalCommitClock clock(RuntimeEpoch(17));
    const auto command = operation(InsertObjectsOp{{section(), child}});
    const auto applied = OperationEngine{}.apply(command, ApplySource::kLocalInteraction, objects, ledger, generation, clock);
    ASSERT_EQ(applied.disposition, ApplyDisposition::kApplied);
    ASSERT_NE(objects.find(child.id), nullptr);
    EXPECT_EQ(*objects.find(child.id), child);
    EXPECT_TRUE(ledger.find(command.id).has_value());
    EXPECT_EQ(objects.children(section().id).size(), 1U);
}
TEST(Fc08SectionA, RejectsSectionSnapshotUntilExplicitCapabilityIsImplemented) {
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId{ObjectId::fromUint64(3)};
    snapshot.schema_version = 1; snapshot.objects = {section()};
    EXPECT_FALSE(SnapshotCodec::encode(snapshot).ok());
}
TEST(Fc08SectionA, SectionSnapshotRequiresExplicitReaderAndRestoresHierarchy) {
    SemanticSnapshot snapshot;
    snapshot.document_id = DocumentId{ObjectId::fromUint64(3)};
    snapshot.schema_version = 3; snapshot.manifest_present = true;
    snapshot.document_revision = 7; snapshot.digest_algorithm = 1;
    p::RecoveryFrontierV1 frontier;
    frontier.set_version(1); frontier.set_kind(1); frontier.set_sequence(7);
    snapshot.recovery_frontier = wire(frontier.SerializeAsString());
    p::CapabilityRequirementsV1 requirements;
    requirements.add_required_ids("resource-manifest-v1");
    requirements.add_required_ids("section-v1");
    snapshot.capability_requirements = wire(requirements.SerializeAsString());
    auto child = section(); child.id = ObjectId::fromUint64(4);
    child.kind = ObjectKind::kShape; child.kind_version = 1;
    child.content = ShapeContent{1, 20, 20}; child.placement.parent_id = section().id;
    child.transform.tx = 43; child.transform.ty = -27;
    snapshot.objects = {section(), child};
    const auto sectionCapability = SnapshotCapability::kShape2TriangleRichTextFormattingConnectorAndSectionV1;
    auto missingRequirement = snapshot;
    p::CapabilityRequirementsV1 legacyRequirements;
    legacyRequirements.add_required_ids("resource-manifest-v1");
    missingRequirement.capability_requirements = wire(legacyRequirements.SerializeAsString());
    EXPECT_FALSE(SnapshotCodec::bindDigests(missingRequirement, sectionCapability));
    EXPECT_FALSE(SnapshotCodec::bindDigests(snapshot, SnapshotCapability::kShape2TriangleRichTextFormattingAndConnectorAttachmentV1));
    auto existing = missingRequirement; existing.objects.clear();
    const auto oldCapability = SnapshotCapability::kShape2TriangleRichTextFormattingAndConnectorAttachmentV1;
    ASSERT_TRUE(SnapshotCodec::bindDigests(existing, oldCapability));
    EXPECT_EQ(SnapshotCodec::encode(existing, oldCapability).bytes,
              SnapshotCodec::encode(existing, sectionCapability).bytes);
    ASSERT_TRUE(SnapshotCodec::bindDigests(snapshot, sectionCapability));
    const auto encoded = SnapshotCodec::encode(snapshot, sectionCapability);
    ASSERT_TRUE(encoded.ok());
    EXPECT_FALSE(SnapshotCodec::decode(encoded.bytes, SnapshotCapability::kShape2TriangleRichTextFormattingAndConnectorAttachmentV1).ok());
    const auto decoded = SnapshotCodec::decode(encoded.bytes, sectionCapability);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(decoded.snapshot->objects, snapshot.objects);
    EXPECT_EQ(decoded.snapshot->capability_requirements, snapshot.capability_requirements);
    DocumentRuntimeState state = DocumentRuntimeState::kLoading;
    IndexedObjectStore store; ResourceManifest manifest; SemanticGenerationState generation;
    SnapshotBootstrapper::CheckpointBinding binding{snapshot.document_id, snapshot.document_revision,
        snapshot.recovery_frontier, snapshot.checkpoint_digest};
    DocumentRuntimeState oldState = DocumentRuntimeState::kLoading;
    IndexedObjectStore oldStore; ResourceManifest oldManifest; SemanticGenerationState oldGeneration;
    EXPECT_FALSE(SnapshotBootstrapper::restore(*decoded.snapshot, oldCapability, oldState,
                                               oldStore, oldManifest, oldGeneration, binding).restored);
    EXPECT_EQ(oldStore.size(), 0U);
    ASSERT_TRUE(SnapshotBootstrapper::restore(*decoded.snapshot, sectionCapability, state,
                                              store, manifest, generation, binding).restored);
    ASSERT_NE(store.find(child.id), nullptr);
    EXPECT_EQ(*store.find(child.id), child);
    // Bootstrap restores into the Loading owner; publication/Ready belongs to
    // the caller and must not be inferred from a successful codec restore.
    EXPECT_EQ(state, DocumentRuntimeState::kLoading);
}
TEST(Fc08SectionA, SectionPropertiesRejectForbiddenDefaultsAndClearWithoutMutation) {
    auto object = section();
    object.properties.entries = {{1, PropertyValue{true}}};
    EXPECT_FALSE(normalizeOperation(operation(InsertObjectsOp{{object}})).ok());
    object = section();
    object.properties.entries = {{3, PropertyValue{0.5F}}};
    EXPECT_TRUE(validatePayloadStructure(operation(InsertObjectsOp{{object}})).ok());
    ReferenceObjectStore objects; AppliedOperationLedger ledger;
    SemanticGenerationState generation; CanonicalCommitClock clock(RuntimeEpoch(17));
    ASSERT_EQ(OperationEngine{}.apply(operation(InsertObjectsOp{{object}}), ApplySource::kLocalInteraction,
        objects, ledger, generation, clock).disposition, ApplyDisposition::kApplied);
    const auto before = objects.allObjects();
    for (const auto field_id : {1U, 2U, 4U, 0x101U}) {
        const auto command = operation(PatchPropertiesOp{{{object.id, field_id, PropertyPatchAction::kClear, {}}}}, 20 + field_id);
        EXPECT_NE(OperationEngine{}.apply(command, ApplySource::kLocalInteraction, objects, ledger,
            generation, clock).disposition, ApplyDisposition::kApplied);
        EXPECT_EQ(objects.allObjects(), before);
        EXPECT_EQ(generation.current().value(), 1U);
        EXPECT_EQ(clock.lastCommittedOrdinal().value(), 1U);
        EXPECT_FALSE(ledger.find(command.id).has_value());
    }
}
} // namespace
