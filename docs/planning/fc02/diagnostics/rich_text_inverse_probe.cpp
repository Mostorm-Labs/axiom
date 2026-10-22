// Design diagnostic: exercise existing Semantic code, not a proposed V2 implementation.
#include "rich_text_state_plan.hpp"
#include "object_record_semantics_internal.hpp"

#include <iostream>
#include <string>

using namespace canvas::semantic;

int main() {
    TextStyle style;
    style.font_resource_id = ResourceId{ObjectId::fromUint64(90)};
    style.font_size = 12;
    style.weight = 400;
    style.color = {0, 0, 0, 1};
    const auto paragraph_id = ObjectId::fromUint64(1);
    RichTextContent original;
    original.document.paragraphs = {
        Paragraph{paragraph_id, {ParagraphAlignment::kLeft, 1, 0, 0},
                  {{"A", style}, {"B", style}}}};
    ObjectRecord record;
    record.id = ObjectId::fromUint64(2);
    record.kind = ObjectKind::kRichText;
    record.kind_version = 1;
    record.placement.order_key = OrderKey{{1}};
    record.content = original;
    const bool normalized = internal::normalizeObjectRecord(record);
    const bool valid = internal::validateObjectRecord(record);
    const auto& admitted = std::get<RichTextContent>(record.content);
    RichTextContent after_insert, after_inverse;
    const auto inserted = internal::prepareRichTextDeltaState(
        admitted, RichTextDelta{1, {InsertTextStep{paragraph_id, 1, "X", style}}},
        &after_insert);
    const auto reversed = internal::prepareRichTextDeltaState(
        after_insert, RichTextDelta{1, {DeleteTextStep{paragraph_id, 1, 1}}},
        &after_inverse);
    if (!normalized || !valid || admitted != original ||
        !inserted.ok() || !reversed.ok()) {
        std::cerr << "PROBE_INVALID: precondition failed\n";
        return 2;
    }
    std::string text;
    for (const auto& run : after_inverse.document.paragraphs.front().runs) text += run.text;
    const bool exact = after_inverse == admitted;
    std::cout << "normalized=true valid=true\n"
              << "before_runs=" << admitted.document.paragraphs.front().runs.size()
              << " inserted_runs=" << after_insert.document.paragraphs.front().runs.size()
              << " inverse_runs=" << after_inverse.document.paragraphs.front().runs.size() << '\n'
              << "inverse_text=" << text << " exact_preimage=" << (exact ? "true" : "false") << '\n';
    if (exact || text != "AB") return 2;
    std::cout << "CONFIRMED: reverse V1 text steps do not guarantee exact canonical Undo\n";
    return 0; // A confirmed design counterexample, not a passing V2 Undo test.
}
