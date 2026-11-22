// Actual EditingSceneCompiler and OperationEngine; full compiler is the oracle.
#define AXIOM_CPU_PENDING_FIXTURE_MAIN stampInheritedFixtureMain
#include "cpu_pending_runtime_test.cpp"

int main(int argc, char** argv) {
  try {
    require(argc == 3, "snapshot output");
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
    auto decoded = semantic::SnapshotCodec::decode(
        bytes, semantic::SnapshotCapability::kAllWithCompactStrokeV1);
    require(decoded.ok(), "stamp fixture decode");
    auto snapshot = *decoded.snapshot;
    auto first = std::find_if(snapshot.objects.begin(), snapshot.objects.end(),
        [](const auto& object) {
          return std::holds_alternative<semantic::CompactStrokeContent>(object.content);
        });
    require(first != snapshot.objects.end(), "stroke fixture");
    auto object = *first;
    object.erase_masks.clear();
    snapshot.objects = {object};
    require(semantic::SnapshotCodec::populateRequirements(snapshot) &&
        semantic::SnapshotCodec::bindDigests(snapshot,
            semantic::SnapshotCapability::kAllWithCompactStrokeV1), "bind snapshot");

    runtime::RuntimeCompositionCore core;
    require(core.openDocument(snapshot.document_id), "open");
    require(core.restoreSnapshotBeforeSurface(snapshot,
        {snapshot.document_id, snapshot.document_revision, snapshot.recovery_frontier,
         snapshot.checkpoint_digest}, 1) == runtime::DocumentRestoreStatus::kRestored,
        "restore");
    require(core.bindSurface(1431, 1010), "bind surface for operation admission");
    auto& editor = core.qualificationRuntime();
    auto& compiler = ink_playground::InkPlaygroundHistoryTestAccess::compiler(editor);
    const auto checkFullScene = [&] {
      auto full = ink_playground::InkPlaygroundHistoryTestAccess::full(editor);
      require(bool(full), "independent full compiler");
      auto view = ink_playground::InkPlaygroundHistoryTestAccess::current(editor);
      for (const auto& record : full.value().records) {
        const auto* actual = view.find(record.objectId);
        require(actual && *actual == record,
            "incremental record differs from full scene/content stamp");
      }
    };
    const auto operation = [&](unsigned id, std::vector<semantic::EraseMaskRecord> masks) {
      return semantic::Operation{
          semantic::OperationId{foundation::ObjectId::fromUint64(id)},
          snapshot.document_id, 1, 1,
          semantic::AddEraseMasksOp{{{object.id, std::move(masks)}}}};
    };
    const auto apply = [&](const semantic::Operation& op) {
      require(editor.applyExternalOperation(op, semantic::ApplySource::kRemoteSync)
          .disposition == semantic::ApplyDisposition::kApplied, "mask operation apply");
      checkFullScene();
    };
    const auto sweep = [](unsigned count) {
      semantic::SweptCircleMask mask;
      for (unsigned n = 0; n < count; ++n) {
        mask.segments.push_back({{{double(n % 100), 20}, 2},
            {{double(n % 100) + 1, 21}, 2}, {double(n % 100), 20},
            {double(n % 100) + 1, 21}});
      }
      return mask;
    };
    const auto prepare = [&](const semantic::Operation& op) {
      auto proof = editor.pendingSceneRecord(object.id);
      require(bool(proof), "published proof");
      auto owner = std::make_shared<const semantic::Operation>(op);
      interaction::PendingSceneStampPreparation cursor;
      unsigned turns = 1;
      while (!interaction::preparePendingMaskStamps(
          compiler, {*proof}, owner, cursor, 1, 1000000)) {
        require(++turns < 1024, "stamp progress");
      }
      // Keep the prepared job alive until the actual apply consumes it.
      return std::pair{turns, std::move(cursor)};
    };
    const auto stamp = [&] {
      auto view = ink_playground::InkPlaygroundHistoryTestAccess::current(editor);
      const auto* record = view.find(object.id);
      require(record != nullptr, "published record");
      return record->contentRevision;
    };

    // Establish historical metadata through the real incremental publication
    // path. A snapshot's independent full compile has no cumulative mask memo.
    auto history = operation(999, {
        {foundation::ObjectId::fromUint64(100), sweep(4096)},
        {foundation::ObjectId::fromUint64(101), sweep(4096)}});
    apply(history);
    auto append = operation(1000,
        {{foundation::ObjectId::fromUint64(200), sweep(64)}});
    auto [appendTurns, appendCursor] = prepare(append);
    apply(append);

    // Front insertion must invalidate every historical cumulative hash.
    auto insert = operation(1001,
        {{foundation::ObjectId::fromUint64(1), sweep(64)}});
    auto [insertTurns, insertCursor] = prepare(insert);
    apply(insert);
    require(insertTurns == 260, "front insertion skipped the changed hash suffix");
    const auto beforeReplacement = stamp();

    // Compensation removes the ID; reapply changes a value but not its count.
    // An ID/count shortcut would incorrectly reuse the previous hash here.
    auto removed = semantic::Operation{
        semantic::OperationId{foundation::ObjectId::fromUint64(1002)},
        snapshot.document_id, 1, 1, semantic::RemoveEraseMasksOp{
          {{object.id, {foundation::ObjectId::fromUint64(200)}}}}};
    apply(removed);
    append.id = semantic::OperationId{foundation::ObjectId::fromUint64(1003)};
    std::get<semantic::SweptCircleMask>(
        std::get<semantic::AddEraseMasksOp>(append.payload).items[0].masks[0].geometry)
        .segments.back().p1.radius = 7;
    auto [replacementTurns, replacementCursor] = prepare(append);
    apply(append);
    require(stamp() != beforeReplacement, "changed same-ID mask kept old stamp");

    // A restored document has no cumulative published hash metadata yet. The
    // first preparation must retain the original bounded full-history path.
    auto coldSnapshot = snapshot;
    coldSnapshot.objects[0].erase_masks =
        std::get<semantic::AddEraseMasksOp>(history.payload).items[0].masks;
    require(semantic::SnapshotCodec::populateRequirements(coldSnapshot) &&
        semantic::SnapshotCodec::bindDigests(coldSnapshot,
            semantic::SnapshotCapability::kAllWithCompactStrokeV1), "cold bind");
    runtime::RuntimeCompositionCore cold;
    require(cold.openDocument(coldSnapshot.document_id), "cold open");
    require(cold.restoreSnapshotBeforeSurface(coldSnapshot,
        {coldSnapshot.document_id, coldSnapshot.document_revision,
         coldSnapshot.recovery_frontier, coldSnapshot.checkpoint_digest}, 1) ==
        runtime::DocumentRestoreStatus::kRestored, "cold restore");
    require(cold.bindSurface(1431, 1010), "cold surface");
    auto& coldEditor = cold.qualificationRuntime();
    auto& coldCompiler = ink_playground::InkPlaygroundHistoryTestAccess::compiler(coldEditor);
    auto coldProof = coldEditor.pendingSceneRecord(object.id);
    require(bool(coldProof), "cold proof");
    auto coldOwner = std::make_shared<const semantic::Operation>(append);
    interaction::PendingSceneStampPreparation coldCursor;
    unsigned coldTurns = 1;
    while (!interaction::preparePendingMaskStamps(coldCompiler, {*coldProof},
        coldOwner, coldCursor, 1, 1000000)) {
      require(++coldTurns < 1024, "cold progress");
    }
    require(coldTurns == 258, "unknown historical hash was reused");

    json report{{"rows", json::array({
        {{"case", "append-existing8192-new64"}, {"turns", appendTurns},
         {"historyMasks", 2}, {"historyPrimitives", 8192}, {"newPrimitives", 64}},
        {{"case", "sorted-front-insertion"}, {"turns", insertTurns}},
        {{"case", "changed-value-after-compensation"}, {"turns", replacementTurns}},
        {{"case", "cold-restored-history"}, {"turns", coldTurns}}
      })}, {"fullSceneEquivalent", true}, {"changedValueStampDiffers", true},
      {"appendPass", appendTurns == 2}, {"pass", appendTurns == 2}};
    std::ofstream(argv[2]) << report.dump(2) << '\n';
    std::cout << report.dump() << '\n';
    return appendTurns == 2 ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
}
