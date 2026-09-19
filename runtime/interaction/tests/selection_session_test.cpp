#include "canvas/interaction/selection_session.hpp"

#include "canvas/foundation/object_id.hpp"
#include "canvas/semantic/change_set.hpp"

#include <cassert>
#include <vector>

namespace {
using canvas::foundation::ObjectId;
using canvas::interaction::HitCandidate;
using canvas::interaction::SelectionSession;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

void click_and_toggle_keep_local_deterministic_state() {
    SelectionSession selection;
    assert(selection.click(id(1)));
    assert(selection.primary() == id(1));
    assert(selection.selected().size() == 1);
    assert(selection.toggle(id(2)));
    assert(selection.selected().size() == 2);
    assert(selection.primary() == id(1));
    assert(selection.toggle(id(1)));
    assert(selection.primary() == id(2));
    assert(selection.selected().size() == 1);
}

void marquee_filters_hidden_and_locked_without_canonical_writes() {
    SelectionSession selection;
    const std::vector<HitCandidate> candidates{{id(1), true, false},
                                               {id(2), false, false},
                                               {id(3), true, true},
                                               {id(4), true, false}};
    assert(selection.marquee(candidates));
    assert(selection.selected().size() == 2);
    assert(selection.selected()[0] == id(1));
    assert(selection.selected()[1] == id(4));
}

void deleted_object_is_pruned_and_cancel_clears_only_local_state() {
    SelectionSession selection;
    assert(selection.click(id(7)));
    assert(selection.toggle(id(8)));
    const auto changes = canvas::semantic::ChangeSet::fromChanges(
        canvas::semantic::SemanticGeneration(1), canvas::semantic::SemanticGeneration(2),
        {{id(7), canvas::semantic::SemanticChangeFlags::kDeleted, {}}});
    selection.onChangeSet(changes);
    assert(selection.selected().size() == 1);
    assert(selection.primary() == id(8));
    selection.cancel();
    assert(selection.selected().empty());
}
} // namespace

int main() {
    click_and_toggle_keep_local_deterministic_state();
    marquee_filters_hidden_and_locked_without_canonical_writes();
    deleted_object_is_pruned_and_cancel_clears_only_local_state();
    return 0;
}
