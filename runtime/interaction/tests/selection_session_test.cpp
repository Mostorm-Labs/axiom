#include "canvas/interaction/selection_session.hpp"

#include "canvas/foundation/object_id.hpp"
#include "canvas/semantic/change_set.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

namespace {
using canvas::foundation::ObjectId;
using canvas::interaction::HitCandidate;
using canvas::interaction::SelectionSession;
using canvas::interaction::SelectionCombineMode;

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

void add_existing_root_does_not_toggle() {
    SelectionSession selection;
    assert(selection.click(id(1)));
    assert(selection.toggle(id(2)));
    const std::vector<ObjectId> hits{id(2), id(3)};
    assert(selection.applyAcquisition(hits, SelectionCombineMode::kAdd, selection.revision()));
    assert((std::vector<ObjectId>(selection.selected().begin(), selection.selected().end()) ==
            std::vector<ObjectId>{id(1), id(2), id(3)}));
    assert(selection.primary() == id(1));
}

void replace_empty_clears_add_empty_preserves() {
    SelectionSession selection;
    assert(selection.click(id(7)));
    const auto revision = selection.revision();
    assert(selection.applyAcquisition({}, SelectionCombineMode::kAdd, revision));
    assert(selection.primary() == id(7) && selection.revision() == revision);
    assert(selection.applyAcquisition({}, SelectionCombineMode::kReplace, revision));
    assert(selection.selected().empty() && selection.revision() > revision);
}

void stale_acquisition_does_not_overwrite() {
    SelectionSession selection;
    assert(selection.click(id(1)));
    const auto revision = selection.revision();
    assert(selection.click(id(2)));
    const std::vector<ObjectId> hits{id(3)};
    assert(!selection.applyAcquisition(hits, SelectionCombineMode::kReplace, revision));
    assert(selection.primary() == id(2));
}

void duplicate_roots_are_unique() {
    SelectionSession selection;
    const std::vector<ObjectId> hits{id(4), id(4), id(2), id(2)};
    assert(selection.applyAcquisition(hits, SelectionCombineMode::kReplace, selection.revision()));
    assert(selection.selected().size() == 2U && selection.primary() == id(4));
    const auto revision = selection.revision();
    const std::vector<ObjectId> invalid{id(9), ObjectId{}};
    assert(!selection.applyAcquisition(invalid, SelectionCombineMode::kReplace, revision));
    assert(selection.primary() == id(4) && selection.revision() == revision);
}

class HierarchyFixture final : public canvas::semantic::ObjectStore {
public:
    std::vector<canvas::semantic::ObjectRecord> records;
    std::size_t size() const noexcept override {return records.size();}
    bool contains(const ObjectId& value) const noexcept override {return find(value)!=nullptr;}
    const canvas::semantic::ObjectRecord* find(const ObjectId& value) const noexcept override {
        for(const auto& r:records)if(r.id==value)return &r;return nullptr;
    }
    std::vector<canvas::semantic::ObjectRecord> allObjects() const override {return records;}
    std::vector<canvas::semantic::ObjectRecord> children(const std::optional<ObjectId>& parent) const override {
        std::vector<canvas::semantic::ObjectRecord> result;
        for(const auto& r:records)if(r.placement.parent_id==parent)result.push_back(r);return result;
    }
    void add(unsigned value,canvas::semantic::ObjectKind kind,std::optional<ObjectId> parent={}) {
        canvas::semantic::ObjectRecord r;r.id=id(value);r.kind=kind;r.kind_version=1;r.placement.parent_id=parent;records.push_back(r);
    }
};
void hierarchy_roots_are_stable_scoped_and_nonredundant() {
    using K=canvas::semantic::ObjectKind;HierarchyFixture store;
    store.add(1,K::kSection);store.add(2,K::kShape,id(1));store.add(3,K::kShape,id(1));store.add(4,K::kShape);
    store.add(5,K::kGroup);store.add(6,K::kGroup,id(5));store.add(7,K::kShape,id(6));store.add(8,K::kShape,id(5));
    store.add(9,K::kSticky);store.add(10,K::kRichText,id(9));
    SelectionSession s;
    for(int n=0;n<3;++n) {
        auto r=s.resolveRoots(std::vector{id(2),id(4),id(1),id(2)},store);
        assert(r && (*r==std::vector{id(4),id(1)}));
        r=s.resolveRoots(std::vector{id(2),id(3)},store);assert(r&&(*r==std::vector{id(2),id(3)}));
        r=s.resolveRoots(std::vector{id(7),id(8),id(5),id(4)},store);assert(r&&(*r==std::vector{id(5),id(4)}));
        r=s.resolveRoots(std::vector{id(10),id(9)},store);assert(r&&(*r==std::vector{id(9)}));
    }
    assert(s.enterGroupScope(id(5),store));
    auto r=s.resolveRoots(std::vector{id(7),id(8),id(4)},store);assert(r&&(*r==std::vector{id(6),id(8)}));
    assert(s.enterGroupScope(id(6),store));
    r=s.resolveRoots(std::vector{id(7),id(8),id(4)},store);assert(r&&(*r==std::vector{id(7)}));
    assert(s.selected().empty());
}
void missing_or_cyclic_ancestors_are_explicitly_incomplete() {
    using K=canvas::semantic::ObjectKind;HierarchyFixture store;SelectionSession s;
    store.add(1,K::kShape,id(99));assert(!s.resolveRoots(std::vector{id(1)},store));
    assert(!s.resolveRoots(std::vector{id(99)},store));
    store.add(99,K::kGroup,id(99));assert(!s.resolveRoots(std::vector{id(1)},store));
    assert(s.selected().empty());
}
} // namespace

int main() {
    click_and_toggle_keep_local_deterministic_state();
    marquee_filters_hidden_and_locked_without_canonical_writes();
    deleted_object_is_pruned_and_cancel_clears_only_local_state();
    add_existing_root_does_not_toggle();
    replace_empty_clears_add_empty_preserves();
    stale_acquisition_does_not_overwrite();
    duplicate_roots_are_unique();
    hierarchy_roots_are_stable_scoped_and_nonredundant();
    missing_or_cyclic_ancestors_are_explicitly_incomplete();
    return 0;
}
