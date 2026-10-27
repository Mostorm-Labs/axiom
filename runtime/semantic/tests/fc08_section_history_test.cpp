#include "fc08_container_fixture.hpp"
#include "canvas/interaction/editor_history.hpp"
#include "undoable_command.hpp"
#include "object_store_mutator.hpp"
#include <limits>
#include <cstdio>
namespace {
using namespace fc08_test;
using namespace canvas::interaction;
template<class Store> struct HistoryRuntime : Runtime<Store>, HistorySubmitPort {
    EditorHistory history{*this};
    std::uint64_t nonce = 0x8000000000000000ULL;
    bool frozenIdSequence = false;
    ApplyDisposition last = ApplyDisposition::kRejected;
    std::vector<OperationId> submitted;
    std::size_t publicationCallbacks=0, publishedHistorySize=0;
    bool publishedCanUndo=false, publishedCanRedo=false;
    void publishAdmitted() noexcept override {
        ++publicationCallbacks; publishedHistorySize=history.size();
        publishedCanUndo=history.canUndo();publishedCanRedo=history.canRedo();
    }
    OperationId allocateOperationId() override {
        if (!frozenIdSequence) return OperationId{ObjectId::fromUint64(nonce++)};
        ObjectId value; const auto n=nonce++;
        for(unsigned i=0;i<8U;++i) value.bytes[15U-i]=static_cast<std::uint8_t>(n>>(i*8U));
        return OperationId{value};
    }
    SubmitResult submit(std::span<const Operation>, ApplySource) override { return SubmitResult::rejected(); }
    SubmitResult submitAdmitted(const Operation& operation, ApplySource source, OperationAdmission& admission) override {
        submitted.reserve(submitted.size() + 1U);
        const auto result = this->engine.apply(operation, source, this->objects, this->ledger,
            this->generation, this->clock, &admission);
        last = result.disposition;
        if (last == ApplyDisposition::kApplied) submitted.push_back(operation.id);
        return last == ApplyDisposition::kApplied || last == ApplyDisposition::kAlreadyApplied ?
            SubmitResult::acceptedResult() : SubmitResult::rejected();
    }
};
Operation forwardModel(const Json& row, const Json& input) {
    const auto wire = wireFor(row, "forward");
    if (row["variant_id"] != "FC08-T06/payload-permutation") {
        const auto d = SemanticCodec::decodeProtobufOperation(wire);
        if (!d.ok()) throw std::invalid_argument("forward");
        return d.operation;
    }
    p::Operation dto; literal(input["forward"], dto); Operation op;
    op.id = OperationId{id(input["forward"]["operation_id"]["value"].get<std::string>())};
    op.document_id = DocumentId{id(input["forward"]["document_id"]["value"].get<std::string>())};
    op.schema_version = 1; op.payload_version = 1;
    if (!canvas::semantic::internal::fromProtobufContainerOperation(dto.payload(), op.payload))
        throw std::invalid_argument("typed forward");
    return op;
}
class SectionHistoryFrozen : public testing::TestWithParam<Json> {};
template<class Store> void frozenHistory(const Json& row) {
    SCOPED_TRACE(row["variant_id"].get<std::string>());
    const auto input = read(root / row["input"]["path"].get<std::string>());
    const auto expected = read(root / row["expected"]["path"].get<std::string>());
    HistoryRuntime<Store> rt; rt.seed(input["before"]);
    if (input.contains("intervening_operation") && !input["intervening_operation"].is_null())
        ASSERT_EQ(rt.apply(carrier(input["intervening_operation"])).disposition, ApplyDisposition::kApplied);
    const auto before = records(rt.objects);
    const auto generation = rt.generation.current().value();
    const auto ordinal = rt.clock.lastCommittedOrdinal().value();
    bool shouldApply = true;
    for (const auto& predicate : expected["predicates"])
        if (predicate["op"] == "equal" && predicate["path"] == "outcome")
            shouldApply = predicate["expected"] == "Applied";
    if (row["variant_id"] != "FC08-T06/payload-permutation") {
        const auto decoded = SemanticCodec::decodeProtobufOperation(wireFor(row, "forward"));
        if (!decoded.ok()) {
            ASSERT_FALSE(shouldApply); EXPECT_EQ(records(rt.objects), before);
            EXPECT_EQ(rt.history.size(), 0U); EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(), ordinal);
            return;
        }
    }
    const auto forward = forwardModel(row, input);
    const bool accepted = rt.history.executeUndoable(forward);
    std::printf("C forward variant=%s accepted=%d generation=%llu ordinal=%llu history=%zu\n",
        row["variant_id"].get<std::string>().c_str(), accepted,
        static_cast<unsigned long long>(rt.generation.current().value()),
        static_cast<unsigned long long>(rt.clock.lastCommittedOrdinal().value()), rt.history.size());
    ASSERT_EQ(accepted, shouldApply);
    if (!accepted) {
        EXPECT_EQ(records(rt.objects), before); EXPECT_EQ(rt.history.size(), 0U);
        EXPECT_EQ(rt.generation.current().value(), generation);
        EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(), ordinal); return;
    }
    const auto after = records(rt.objects);
    std::printf("C observed forward records=%s\n", after.dump().c_str());
    for (const auto& predicate : expected["predicates"])
        if (predicate["op"] == "equal" && predicate["path"] == "canonical.records")
            EXPECT_EQ(after, predicate["expected"]);
    ASSERT_EQ(rt.history.size(), 1U);
    EXPECT_TRUE(rt.history.executeUndoable(forward));
    EXPECT_EQ(rt.history.size(), 1U); EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(), ordinal + 1U);
    ASSERT_TRUE(rt.history.undo()); EXPECT_EQ(records(rt.objects), before);
    std::printf("C observed undo records=%s\n",records(rt.objects).dump().c_str());
    ASSERT_TRUE(rt.history.redo()); EXPECT_EQ(records(rt.objects), after);
    std::printf("C observed redo records=%s\n",records(rt.objects).dump().c_str());
    EXPECT_EQ(rt.history.size(), 1U); EXPECT_EQ(rt.generation.current().value(), generation + 3U);
    EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(), ordinal + 3U);
    ASSERT_EQ(rt.submitted.size(), 3U);
    EXPECT_NE(rt.submitted[0], rt.submitted[1]); EXPECT_NE(rt.submitted[1], rt.submitted[2]);
    ASSERT_TRUE(rt.history.undo()); EXPECT_EQ(records(rt.objects), before);
    if constexpr (std::is_same_v<Store, IndexedObjectStore>)
        EXPECT_TRUE(canvas::semantic::internal::ObjectStoreMutator::indexMatchesRebuild(rt.objects));
}
TEST_P(SectionHistoryFrozen,ReferenceOwner) { frozenHistory<ReferenceObjectStore>(GetParam()); }
TEST_P(SectionHistoryFrozen,IndexedOwner) { frozenHistory<IndexedObjectStore>(GetParam()); }
INSTANTIATE_TEST_SUITE_P(Frozen, SectionHistoryFrozen, testing::ValuesIn(cases(false)),
    [](const testing::TestParamInfo<Json>& i) { return std::to_string(i.index); });
TEST(SectionHistory,RealPrecommitHistoryEntryExists) {
    EXPECT_TRUE(supportsUndoableCommand(OperationKind::kEditContainerTopology));
}
ObjectRecord section(std::uint64_t value = 2) {
    ObjectRecord r; r.id = ObjectId::fromUint64(value); r.kind = ObjectKind::kSection;
    r.kind_version = 1; r.placement.order_key = OrderKey({static_cast<std::uint8_t>(value)});
    r.content = SectionContent{{0,0,100,80},"Section",4,16}; return r;
}
Operation operation(OperationPayload payload, std::uint64_t value = 100) {
    return {OperationId{ObjectId::fromUint64(value)},DocumentId{ObjectId::fromUint64(200)},1,1,std::move(payload)};
}
Operation rename(const ObjectRecord& r, const std::string& title, std::uint64_t value = 101) {
    auto content = std::get<SectionContent>(r.content); auto next = content; next.title = title;
    return operation(SetSectionContentOp{{{r.id, 1, content, next}},{}}, value);
}
TEST(SectionHistory,SetSectionContentUsesRealInverseAndPreservesRedoOnRejection) {
    HistoryRuntime<IndexedObjectStore> rt;
    ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{section()}})));
    ASSERT_TRUE(rt.history.executeUndoable(rename(*rt.objects.find(section().id), "new title")));
    ASSERT_TRUE(rt.history.undo());
    const auto before = rt.objects.allObjects(); const auto ordinal = rt.clock.lastCommittedOrdinal();
    auto stale = rename(section(), "rejected"); std::get<SetSectionContentOp>(stale.payload).items[0].expected_content.title = "wrong";
    EXPECT_FALSE(rt.history.executeUndoable(stale)); EXPECT_TRUE(rt.history.canRedo());
    EXPECT_EQ(rt.objects.allObjects(), before); EXPECT_EQ(rt.clock.lastCommittedOrdinal(), ordinal);
    ASSERT_TRUE(rt.history.redo()); EXPECT_EQ(std::get<SectionContent>(rt.objects.find(section().id)->content).title, "new title");
}
TEST(SectionHistory,SectionBoundaryInverseUsesRealPostStateGeometryBasis) {
    HistoryRuntime<IndexedObjectStore> rt; const auto r = section();
    ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{r}})));
    auto next = std::get<SectionContent>(r.content); next.boundary = {-10,-20,200,160};
    auto op = operation(SetSectionContentOp{{{r.id,1,std::get<SectionContent>(r.content),next}},{{r.id,r.kind,r.kind_version,r.placement,r.transform}}},102);
    ASSERT_TRUE(rt.history.executeUndoable(op)); ASSERT_TRUE(rt.history.undo()); EXPECT_EQ(*rt.objects.find(r.id), r);
    ASSERT_TRUE(rt.history.redo()); EXPECT_EQ(std::get<SectionContent>(rt.objects.find(r.id)->content), next);
}
TEST(SectionHistory,DeleteUsesActualClosureAndRestoreAndConnectorIndex) {
    HistoryRuntime<IndexedObjectStore> rt; auto wrapper = section();
    auto child = wrapper; child.id = ObjectId::fromUint64(3); child.kind = ObjectKind::kShape;
    child.content = ShapeContent{1,20,20}; child.placement.parent_id = wrapper.id;
    auto connector = child; connector.id = ObjectId::fromUint64(4); connector.kind = ObjectKind::kConnector;
    connector.placement.parent_id.reset(); connector.placement.order_key = OrderKey({4});
    connector.content = ConnectorContent{ConnectorEndpoint{AttachedEndpoint{child.id,StablePortAnchor{1}}},ConnectorEndpoint{FreePointEndpoint{{200,100}}},ConnectorRouting::kStraight};
    ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{wrapper, child, connector}})));
    const auto original = rt.objects.allObjects();
    ASSERT_TRUE(rt.history.executeUndoable(operation(DeleteObjectsOp{{wrapper.id}},103)));
    EXPECT_EQ(rt.objects.size(),0U); ASSERT_TRUE(rt.history.undo()); EXPECT_EQ(rt.objects.allObjects(), original);
    EXPECT_TRUE(canvas::semantic::internal::ObjectStoreMutator::indexMatchesRebuild(rt.objects));
    ASSERT_TRUE(rt.history.redo()); EXPECT_EQ(rt.objects.size(),0U);
}
TEST(SectionHistory,LateChildMakesUndoRejectWithoutAbsorbingIt) {
    HistoryRuntime<IndexedObjectStore> rt; const auto r = section();
    ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{r}})));
    auto child = r; child.id = ObjectId::fromUint64(3); child.kind = ObjectKind::kShape;
    child.content = ShapeContent{1,20,20}; child.placement.parent_id = r.id;
    ASSERT_EQ(rt.engine.apply(operation(InsertObjectsOp{{child}},104),ApplySource::kRemoteSync,rt.objects,rt.ledger,rt.generation,rt.clock).disposition,ApplyDisposition::kApplied);
    const auto before = rt.objects.allObjects(); const auto ordinal = rt.clock.lastCommittedOrdinal();
    EXPECT_FALSE(rt.history.undo()); EXPECT_TRUE(rt.history.canUndo()); EXPECT_FALSE(rt.history.canRedo());
    EXPECT_EQ(rt.objects.allObjects(), before); EXPECT_EQ(rt.clock.lastCommittedOrdinal(), ordinal);
}
TEST(SectionHistory,ReusedMoveAndPropertiesRoundTripThroughRealHistory) {
    HistoryRuntime<IndexedObjectStore> rt; auto r = section(); r.properties.entries={{3U,PropertyValue{0.8F}}};
    ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{r}})));
    auto transform = r.transform; transform.tx = 47; transform.ty = -31;
    ASSERT_TRUE(rt.history.executeUndoable(operation(SetTransformsOp{{{r.id,transform}}},105)));
    ASSERT_TRUE(rt.history.undo()); EXPECT_EQ(rt.objects.find(r.id)->transform,r.transform);
    ASSERT_TRUE(rt.history.redo()); EXPECT_EQ(rt.objects.find(r.id)->transform,transform);
    ASSERT_TRUE(rt.history.executeUndoable(operation(PatchPropertiesOp{{{r.id,3U,PropertyPatchAction::kSet,PropertyValue{0.5F}}}},106)));
    ASSERT_TRUE(rt.history.undo()); EXPECT_EQ(rt.objects.find(r.id)->properties,r.properties);
    ASSERT_TRUE(rt.history.redo()); EXPECT_EQ(std::get<float>(rt.objects.find(r.id)->properties.entries[0].value),0.5F);
}
TEST(SectionHistory,SmallDeleteWithOversizedRealInverseRejectsBeforeWrite) {
    HistoryRuntime<IndexedObjectStore> rt; std::vector<ObjectId> ids;
    for (std::uint64_t value = 2; value < 35; ++value) {
        auto r = section(value); std::get<SectionContent>(r.content).title.assign(1024U*1024U,'x');
        ASSERT_EQ(rt.engine.apply(operation(InsertObjectsOp{{r}},200+value),ApplySource::kRestoreReplay,rt.objects,rt.ledger,rt.generation,rt.clock).disposition,ApplyDisposition::kApplied);
        ids.push_back(r.id);
    }
    const auto ordinal = rt.clock.lastCommittedOrdinal(); const auto gen = rt.generation.current();
    auto command = operation(DeleteObjectsOp{ids},300);
    EXPECT_LT(SemanticCodec::measureProtobufOperation(command).bytes, 1024U);
    const auto plan = rt.engine.prepare(command,{rt.objects,rt.ledger}); ASSERT_TRUE(plan.plan);
    RestoreObjectsOp inverse; for (const auto& id : plan.plan->deletes) inverse.objects.push_back(*rt.objects.find(id));
    const auto measure = SemanticCodec::measureProtobufOperation(operation(std::move(inverse),301));
    EXPECT_EQ(measure.error,SemanticError::kLimitExceeded); EXPECT_GT(measure.bytes,33554432U);
    std::printf("C inverse-budget actual=%zu limit=33554432 forward=%zu\n",measure.bytes,SemanticCodec::measureProtobufOperation(command).bytes);
    EXPECT_FALSE(rt.history.executeUndoable(command)); EXPECT_EQ(rt.objects.size(),33U);
    EXPECT_EQ(rt.generation.current(),gen); EXPECT_EQ(rt.clock.lastCommittedOrdinal(),ordinal);
    EXPECT_FALSE(rt.ledger.find(command.id)); EXPECT_EQ(rt.history.size(),0U);
}
TEST(SectionHistory,CompleteMeasurementMatchesFrozenCanonicalWire) {
    std::size_t measured = 0;
    for (const auto& row : cases(false)) {
        const auto wire = wireFor(row,"forward"); const auto decoded = SemanticCodec::decodeProtobufOperation(wire);
        if (!decoded.ok()) continue;
        const auto measure = SemanticCodec::measureProtobufOperation(decoded.operation);
        ASSERT_TRUE(measure.ok()); EXPECT_EQ(measure.bytes,wire.size()); ++measured;
    }
    EXPECT_GT(measured,20U);
}
TEST(SectionHistory,OverLimitStringIsRejectedAndNoOpDoesNotCreateHistory) {
    HistoryRuntime<IndexedObjectStore> rt; auto r = section();
    ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{r}})));
    const auto ordinal = rt.clock.lastCommittedOrdinal();
    EXPECT_FALSE(rt.history.executeUndoable(rename(r,"Section",401)));
    EXPECT_FALSE(rt.history.executeUndoable(rename(r,std::string(1024U*1024U+1U,'x'),402)));
    EXPECT_EQ(rt.history.size(),1U); EXPECT_EQ(rt.clock.lastCommittedOrdinal(),ordinal);
}
TEST(SectionHistory,OldPostcommitRecordCannotAdmitTopology) {
    HistoryRuntime<IndexedObjectStore> rt;
    auto forward = operation(EditContainerTopologyOp{},501);
    EXPECT_FALSE(rt.history.record(forward,{operation(DeleteObjectsOp{{section().id}},502)}));
    EXPECT_EQ(rt.history.size(),0U);
}

TEST(SectionHistory,TopologyUndoRedoPreservesUnrelatedLeafContent) {
    const auto rows = cases(false);
    const auto selected=std::find_if(rows.begin(),rows.end(),[](const auto& v){return v["case_id"]=="FC08-T02";});
    ASSERT_NE(selected,rows.end()); const auto row=*selected; const auto input = read(root/row["input"]["path"].get<std::string>());
    HistoryRuntime<IndexedObjectStore> rt; rt.seed(input["before"]);
    const auto forward = forwardModel(row,input); ASSERT_TRUE(rt.history.executeUndoable(forward));
    auto values=rt.objects.allObjects();
    const auto leaf=std::find_if(values.begin(),values.end(),[](const auto& r){return r.kind==ObjectKind::kShape;});
    ASSERT_NE(leaf,values.end()); const auto originalPlacement = leaf->placement;
    ASSERT_EQ(rt.engine.apply(operation(SetObjectSizeOp{{{leaf->id,123,87}}},600),ApplySource::kRemoteSync,rt.objects,rt.ledger,rt.generation,rt.clock).disposition,ApplyDisposition::kApplied);
    const auto changedContent=rt.objects.find(leaf->id)->content;
    ASSERT_TRUE(rt.history.undo()); EXPECT_EQ(rt.objects.find(leaf->id)->content,changedContent);
    ASSERT_TRUE(rt.history.redo()); EXPECT_EQ(rt.objects.find(leaf->id)->content,changedContent);
    EXPECT_EQ(rt.objects.find(leaf->id)->placement,originalPlacement);
}
TEST(SectionHistory,KeyedBatchOverLimitRejectsBeforeWrite) {
    HistoryRuntime<IndexedObjectStore> rt; SetTransformsOp payload;
    for(std::uint64_t i=1;i<=65536U;++i) payload.items.push_back({ObjectId::fromUint64(i),Transform2D{}});
    auto command=operation(std::move(payload),601);
    EXPECT_FALSE(SemanticCodec::measureProtobufOperation(command).ok());
    EXPECT_FALSE(rt.history.executeUndoable(command)); EXPECT_EQ(rt.objects.size(),0U);
    EXPECT_EQ(rt.history.size(),0U); EXPECT_EQ(rt.generation.current().value(),0U);
    EXPECT_FALSE(rt.ledger.find(command.id));
}

TEST(SectionHistory,RecordByteLimitAppliesBeforeLocalCanonicalPublication) {
    HistoryRuntime<IndexedObjectStore> rt; auto r=section(); r.kind=ObjectKind::kRichText;
    RichTextContent text; Paragraph paragraph; paragraph.id=ObjectId::fromUint64(700);
    paragraph.style.alignment=ParagraphAlignment::kLeft; paragraph.style.line_height=1.2;
    for(unsigned i=0;i<17U;++i) {
        TextStyle style; style.font_resource_id=ResourceId{ObjectId::fromUint64(0xFC0202)}; style.font_size=16+(i%2); style.weight=400; style.color={0,0,0,1};
        paragraph.runs.push_back({std::string(1024U*1024U,'x'),style});
    }
    text.document.paragraphs.push_back(std::move(paragraph)); r.content=std::move(text);
    auto command=operation(InsertObjectsOp{{r}},701);
    const auto measure=SemanticCodec::measureProtobufOperation(command);
    EXPECT_EQ(measure.error,SemanticError::kLimitExceeded);
    EXPECT_GT(measure.bytes,16777216U); EXPECT_LT(measure.bytes,33554432U);
    std::printf("C record-budget actual-operation=%zu record-limit=16777216\n",measure.bytes);
    EXPECT_FALSE(rt.history.executeUndoable(command)); EXPECT_EQ(rt.objects.size(),0U);
    EXPECT_EQ(rt.history.size(),0U); EXPECT_FALSE(rt.ledger.find(command.id));
}

std::vector<Json> historyCases() {
    std::vector<Json> values;
    for(const auto& row:manifest()["variants"])
        if(row["case_id"]=="FC08-H01" || row["case_id"]=="FC08-H02") values.push_back(row);
    return values;
}
class SectionHistoryAccepted : public testing::TestWithParam<Json> {};
template<class Store> void acceptedHistory(const Json& row) {
    SCOPED_TRACE(row["variant_id"].get<std::string>());
    const auto input=read(root/row["input"]["path"].get<std::string>());
    const auto expected=read(root/row["expected"]["path"].get<std::string>());
    HistoryRuntime<Store> rt;rt.nonce=1002;rt.frozenIdSequence=true;rt.seed(input["before"]);
    if(input.contains("setup")) for(const auto& setup:input["setup"])
        ASSERT_EQ(rt.apply(carrier(setup["operation"])).disposition,ApplyDisposition::kApplied);
    const auto ordinal=rt.clock.lastCommittedOrdinal().value();
    const auto wire=carrier(input["forward"]);const auto decoded=SemanticCodec::decodeProtobufOperation(wire);
    ASSERT_TRUE(decoded.ok());const auto forward=decoded.operation;
    const bool accepted=rt.history.executeUndoable(forward);
    Json observed=Json::object();
    const auto cursor=[&](){return rt.history.canUndo()?1U:0U;};
    if(row["case_id"]=="FC08-H01") {
        if(rt.history.size()) {
            ASSERT_TRUE(accepted);
            Json timeline=Json::array({Json{{"size",rt.history.size()},{"cursor",cursor()}}});
            ASSERT_TRUE(rt.history.undo());observed["undo_records"]=records(rt.objects);
            timeline.push_back({{"size",rt.history.size()},{"cursor",cursor()}});
            ASSERT_TRUE(rt.history.redo());timeline.push_back({{"size",rt.history.size()},{"cursor",cursor()}});
            observed["history_timeline"]=timeline;
            Json ids=Json::array();for(const auto& value:rt.submitted) {
                const auto& raw=value.value().bytes;
                ids.push_back({{"value",hex(std::string(raw.begin(),raw.end()))}});
            }
            observed["emitted_operation_ids"]=ids;
        }
        observed["history.size"]=rt.history.size();observed["history.cursor"]=cursor();
        observed["canonical_commit_delta"]=rt.clock.lastCommittedOrdinal().value()-ordinal;
    } else {
        ASSERT_TRUE(accepted);ASSERT_TRUE(input.contains("intervening_operation"));
        ASSERT_EQ(rt.apply(carrier(input["intervening_operation"])).disposition,ApplyDisposition::kApplied);
        const auto beforeUndo=rt.objects.allObjects();const auto gen=rt.generation.current();const auto stamp=rt.clock.lastCommittedOrdinal();
        const bool undone=rt.history.undo();observed["outcome"]=undone?"Applied":"HistoryConflict";
        observed["history.size"]=rt.history.size();observed["history.cursor"]=cursor();
        if(!undone) {
            EXPECT_EQ(rt.objects.allObjects(),beforeUndo);EXPECT_EQ(rt.generation.current(),gen);EXPECT_EQ(rt.clock.lastCommittedOrdinal(),stamp);
        } else {
            for(const auto& r:rt.objects.allObjects()) if(const auto* shape=std::get_if<ShapeContent>(&r.content))
                if(shape->width==30 && shape->height==40) observed["retained_leaf_size"]=Json::array({shape->width,shape->height});
        }
    }
    for(const auto& predicate:expected["predicates"]) {
        if(predicate["op"]=="equal") {
            const auto name=predicate["path"].get<std::string>();ASSERT_TRUE(observed.contains(name))<<name;
            EXPECT_EQ(observed[name],predicate["expected"])<<name;
        } else if(predicate["op"]=="unchanged") {
            ASSERT_EQ(row["case_id"],"FC08-H02"); // compared at the real Undo boundary above
        } else FAIL()<<predicate.dump();
    }
    std::printf("C accepted variant=%s observations=%s\n",row["variant_id"].get<std::string>().c_str(),observed.dump().c_str());
}
TEST_P(SectionHistoryAccepted,ReferenceOwner){acceptedHistory<ReferenceObjectStore>(GetParam());}
TEST_P(SectionHistoryAccepted,IndexedOwner){acceptedHistory<IndexedObjectStore>(GetParam());}
INSTANTIATE_TEST_SUITE_P(Accepted,SectionHistoryAccepted,testing::ValuesIn(historyCases()),
    [](const testing::TestParamInfo<Json>& i){return std::to_string(i.index);});

std::vector<Json> budgetCases() {
    std::vector<Json> values;
    for(const auto& row:manifest()["variants"]) {
        const auto variant=row["variant_id"].get<std::string>();
        if(row["case_id"]=="FC08-H03" || (row["case_id"]=="FC08-H04" && variant.find("/operation-")!=std::string::npos)) values.push_back(row);
    }
    return values;
}
class SectionHistoryBudget : public testing::TestWithParam<Json> {};
TEST_P(SectionHistoryBudget,ActualBidirectionalAdmission) {
    const auto row=GetParam();const auto variant=row["variant_id"].get<std::string>();SCOPED_TRACE(variant);
    const auto expected=read(root/row["expected"]["path"].get<std::string>());
    const auto name=variant.substr(variant.find('/')+1);
    const bool amended=row["case_id"]=="FC08-H03";
    const auto path=root/(amended?"out/fc08-section-c/h03-input-overlay-v0.1":"out/fc08-section-c/frozen-budgets");
    if(amended) {
        const auto overlay=read(root/".aegis/packages/FC-08-SECTION-C-H03-v0.1/input-overlay.json");
        ASSERT_EQ(overlay["status"],"ACCEPTED_BY_USER_FOR_C_VERIFICATION");
        std::printf("C H03 active input=H03-v0.1 expected=%s original-input-preserved=%s\n",
            row["expected"]["sha256"].get<std::string>().c_str(),row["input"]["sha256"].get<std::string>().c_str());
    }
    const auto forward=bytes(path/(name+".pb"));ASSERT_FALSE(forward.empty());
    HistoryRuntime<IndexedObjectStore> rt;
    if(row["case_id"]=="FC08-H03") {
        const auto seedBytes=bytes(path/(name+".seed.pb"));
        const auto seed=SemanticCodec::decodeProtobufOperation(seedBytes);
        ASSERT_TRUE(seed.ok())<<"seed decode error="<<static_cast<int>(seed.error)<<" bytes="<<seedBytes.size();
        const auto prepared=rt.engine.prepare(seed.operation,{rt.objects,rt.ledger});
        ASSERT_EQ(prepared.disposition,PrepareDisposition::kPrepared)<<"seed prepare issue="<<static_cast<int>(prepared.error.issue);
        ASSERT_EQ(rt.apply(seedBytes).disposition,ApplyDisposition::kApplied);
    }
    const auto before=rt.objects.allObjects();const auto ordinal=rt.clock.lastCommittedOrdinal();const auto generation=rt.generation.current();
    const auto trace=[&](const char* phase) {
        const Json observed={{"variant_id",variant},{"input_version",amended?"H03-v0.1":"original"},
            {"phase",phase},{"objects",rt.objects.size()},{"generation",rt.generation.current().value()},
            {"ordinal",rt.clock.lastCommittedOrdinal().value()},{"history_size",rt.history.size()},
            {"can_undo",rt.history.canUndo()},{"can_redo",rt.history.canRedo()},
            {"applied_submissions",rt.submitted.size()},{"last_history_submit_disposition",static_cast<int>(rt.last)}};
        std::printf("C budget SUT observation=%s\n",observed.dump().c_str());
    };
    trace("before-forward");
    bool admitted=false;
    const auto decoded=SemanticCodec::decodeProtobufOperation(forward);
    if(amended) {
        ASSERT_TRUE(decoded.ok())<<"amended forward must be valid before inverse-budget admission";
        const auto prepared=rt.engine.prepare(decoded.operation,{rt.objects,rt.ledger});
        ASSERT_EQ(prepared.disposition,PrepareDisposition::kPrepared);ASSERT_TRUE(prepared.plan);
        EXPECT_EQ(prepared.plan->creates.size(),1U);EXPECT_TRUE(prepared.plan->deletes.empty());
        EXPECT_TRUE(prepared.plan->replacements.empty());
    }
    if(decoded.ok()) admitted=rt.history.executeUndoable(decoded.operation);
    trace("after-forward-attempt");
    bool independentSizePredicate=SemanticCodec::preflightOperationBytes(forward)==SemanticError::kNone;
    if(row["case_id"]=="FC08-H03") independentSizePredicate=rt.objects.size()<=65535U && before.size()+1U<=65535U;
    for(const auto& predicate:expected["predicates"]) {
        ASSERT_EQ(predicate["op"],"equal");
        if(predicate["path"]=="command_admitted") EXPECT_EQ(admitted,predicate["expected"].get<bool>());
        else if(predicate["path"]=="independent_size_predicate") EXPECT_EQ(independentSizePredicate,predicate["expected"].get<bool>());
        else FAIL()<<predicate.dump();
    }
    if(!admitted) {
        EXPECT_EQ(rt.objects.allObjects(),before);EXPECT_EQ(rt.history.size(),0U);
        EXPECT_EQ(rt.generation.current(),generation);EXPECT_EQ(rt.clock.lastCommittedOrdinal(),ordinal);
        if(decoded.ok()) EXPECT_FALSE(rt.ledger.find(decoded.operation.id));
        EXPECT_FALSE(rt.history.canUndo());EXPECT_FALSE(rt.history.canRedo());
        if(amended) { EXPECT_EQ(before.size()+1U,65536U);EXPECT_EQ(rt.last,ApplyDisposition::kRejected); }
    } else {
        const auto after=rt.objects.allObjects();
        if(amended) { EXPECT_EQ(after.size(),65535U);EXPECT_EQ(before.size(),65534U); }
        ASSERT_TRUE(rt.history.undo());EXPECT_EQ(rt.objects.allObjects(),before);
        trace("after-real-undo");
        ASSERT_TRUE(rt.history.redo());EXPECT_EQ(rt.history.size(),1U);
        trace("after-real-redo");
        EXPECT_EQ(rt.objects.allObjects(),after);
        ASSERT_EQ(rt.submitted.size(),3U);
        EXPECT_NE(rt.submitted[0],rt.submitted[1]);EXPECT_NE(rt.submitted[1],rt.submitted[2]);
        EXPECT_NE(rt.submitted[0],rt.submitted[2]);
        Json emitted=Json::array();
        for(const auto& value:rt.submitted) {
            const auto& raw=value.value().bytes;
            emitted.push_back(hex(std::string(raw.begin(),raw.end())));
        }
        std::printf("C budget actual F/U/Redo operation IDs=%s\n",emitted.dump().c_str());
        EXPECT_EQ(rt.generation.current().value(),generation.value()+3U);
        EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(),ordinal.value()+3U);
    }
    std::printf("C budget variant=%s forward-wire=%zu admitted=%d size-predicate=%d history=%zu\n",variant.c_str(),forward.size(),admitted,independentSizePredicate,rt.history.size());
}
INSTANTIATE_TEST_SUITE_P(Accepted,SectionHistoryBudget,testing::ValuesIn(budgetCases()),
    [](const testing::TestParamInfo<Json>& i){return std::to_string(i.index);});

TEST(SectionHistory,H03HistoricalOriginalRecipesRemainInvalid) {
    for(const auto& row:budgetCases())if(row["case_id"]=="FC08-H03") {
        const auto name=row["variant_id"].get<std::string>().substr(9);
        const auto original=root/"out/fc08-section-c/frozen-budgets";
        const auto seedBytes=bytes(original/(name+".seed.pb"));ASSERT_FALSE(seedBytes.empty());
        const auto seed=SemanticCodec::decodeProtobufOperation(seedBytes);
        EXPECT_FALSE(seed.ok());
        std::printf("C historical H03 original remains rejected before setup variant=%s error=%d; not a product budget PASS\n",
            row["variant_id"].get<std::string>().c_str(),static_cast<int>(seed.error));
    }
}

TEST(SectionHistory,ChangedCanonicalBasisAfterHistoryReserveRejectsLocalPublish) {
    struct Runtime final : HistoryRuntime<IndexedObjectStore> {
        bool inject=false;
        SubmitResult submitAdmitted(const Operation& forward,ApplySource source,OperationAdmission& inner) override {
            if(!inject)return HistoryRuntime<IndexedObjectStore>::submitAdmitted(forward,source,inner);
            struct LateAdmission final:OperationAdmission {
                Runtime& runtime;OperationAdmission& inner;
                LateAdmission(Runtime& r,OperationAdmission& a):runtime(r),inner(a){}
                bool admit(const PreparedApplyPlan& plan,const StatefulValidationContext& before,const StagedObjectView& after) override {
                    if(!inner.admit(plan,before,after))return false;
                    auto remote=section(4);
                    return runtime.engine.apply(operation(InsertObjectsOp{{remote}},1004),ApplySource::kRemoteSync,
                        runtime.objects,runtime.ledger,runtime.generation,runtime.clock).disposition==ApplyDisposition::kApplied;
                }
            } admission(*this,inner);
            const auto result=engine.apply(forward,source,objects,ledger,generation,clock,&admission);
            return result.disposition==ApplyDisposition::kApplied?SubmitResult::acceptedResult():SubmitResult::rejected();
        }
    } rt;
    ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{section()}})));
    ASSERT_TRUE(rt.history.undo());ASSERT_TRUE(rt.history.canRedo());
    const auto ordinal=rt.clock.lastCommittedOrdinal();rt.inject=true;
    const auto forward=operation(InsertObjectsOp{{section(3)}},1003);
    EXPECT_FALSE(rt.history.executeUndoable(forward));
    EXPECT_FALSE(rt.objects.contains(section(3).id));EXPECT_TRUE(rt.objects.contains(section(4).id));
    EXPECT_FALSE(rt.ledger.find(forward.id));EXPECT_EQ(rt.history.size(),1U);
    EXPECT_TRUE(rt.history.canRedo());EXPECT_FALSE(rt.history.canUndo());
    EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(),ordinal.value()+1U); // the real intervening commit only
}

TEST(SectionHistory,PublicationFollowsReservedHistoryAndCursorCommit) {
    HistoryRuntime<IndexedObjectStore> rt;
    ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{section()}})));
    EXPECT_EQ(rt.publishedHistorySize,1U);EXPECT_TRUE(rt.publishedCanUndo);EXPECT_FALSE(rt.publishedCanRedo);
    ASSERT_TRUE(rt.history.undo());EXPECT_FALSE(rt.publishedCanUndo);EXPECT_TRUE(rt.publishedCanRedo);
    ASSERT_TRUE(rt.history.redo());EXPECT_TRUE(rt.publishedCanUndo);EXPECT_FALSE(rt.publishedCanRedo);
    EXPECT_EQ(rt.publicationCallbacks,3U);
}

TEST(SectionHistory,CompanionRootMembershipBudgetWithValidKeys) {
    // Companion verification of the same published limits. It is NOT a
    // replacement or PASS for the immutable, currently invalid H03 fixtures.
    const auto key=[](std::size_t ordinal){return OrderKey({static_cast<std::uint8_t>(ordinal>>16U),static_cast<std::uint8_t>(ordinal>>8U),static_cast<std::uint8_t>(ordinal),1});};
    for(const std::size_t count:{65534U,65535U}) {
        HistoryRuntime<IndexedObjectStore> rt;std::vector<ObjectRecord> records;
        records.reserve(count);
        for(std::size_t i=0;i<count;++i) {
            auto r=section();r.id=ObjectId::fromUint64(100+i);r.kind=ObjectKind::kShape;
            r.content=ShapeContent{1,20,20};r.placement.order_key=key(i+1);records.push_back(std::move(r));
        }
        ASSERT_EQ(rt.engine.apply(operation(InsertObjectsOp{records},900),ApplySource::kRestoreReplay,
            rt.objects,rt.ledger,rt.generation,rt.clock).disposition,ApplyDisposition::kApplied);
        auto wrapper=section();wrapper.id=ObjectId::fromUint64(90000);wrapper.placement.order_key=key(count+1);
        EditContainerTopologyOp payload;payload.effect=IntroduceWrappersEffect{{wrapper}};
        ContainerChildrenExpectation rootSet;
        for(const auto& r:records)rootSet.complete_members.push_back({r.id,r.placement.order_key});
        payload.guard.child_sets.push_back(std::move(rootSet));
        const auto before=rt.clock.lastCommittedOrdinal();const auto command=operation(std::move(payload),901);
        const bool admitted=rt.history.executeUndoable(command);
        EXPECT_EQ(admitted,count==65534U);
        if(admitted) { ASSERT_TRUE(rt.history.undo());EXPECT_EQ(rt.objects.size(),count);ASSERT_TRUE(rt.history.redo()); }
        else { EXPECT_EQ(rt.objects.size(),count);EXPECT_EQ(rt.history.size(),0U);EXPECT_EQ(rt.clock.lastCommittedOrdinal(),before);EXPECT_FALSE(rt.ledger.find(command.id)); }
        std::printf("C VALID-KEY-COMPANION root=%zu admitted=%d (not frozen H03 qualification)\n",count,admitted);
    }
}

}
