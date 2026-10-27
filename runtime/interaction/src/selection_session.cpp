#include "canvas/interaction/selection_session.hpp"

#include <algorithm>
#include <cstdint>

namespace canvas::interaction {
foundation::ObjectId SelectionSession::resolveHit(foundation::ObjectId id,const semantic::ObjectStore& store) const {
    if(const auto* child=store.find(id);child&&child->kind==semantic::ObjectKind::kRichText&&child->placement.parent_id) {
        if(const auto* parent=store.find(*child->placement.parent_id);parent&&parent->kind==semantic::ObjectKind::kSticky)id=parent->id;
    }
    auto result=id;auto current=id;bool within=!groupScope_;
    for(std::size_t i=0;i<=store.size();++i) {
        if(groupScope_&&current==*groupScope_){within=true;break;}
        const auto* r=store.find(current);if(!r)return {};
        for(const auto& p:r->properties.entries) {
            if(p.field_id==1U)if(const auto* value=std::get_if<bool>(&p.value);value&&!*value)return {};
        }
        if(r->kind==semantic::ObjectKind::kGroup)result=current;
        if(!r->placement.parent_id)break;current=*r->placement.parent_id;
    }
    return within?result:foundation::ObjectId{};
}
std::optional<std::vector<foundation::ObjectId>> SelectionSession::resolveRoots(
    std::span<const foundation::ObjectId> ids,const semantic::ObjectStore& store) const {
    if(groupScope_) {
        const auto* scope=store.find(*groupScope_);
        if(!scope||scope->kind!=semantic::ObjectKind::kGroup)return std::nullopt;
    }
    std::vector<foundation::ObjectId> resolved;
    for(auto id:ids) {
        const auto* record=store.find(id);if(!record)return std::nullopt;
        // Reject incomplete/cyclic ancestry rather than silently manufacturing roots.
        std::size_t depth=0;
        while(record->placement.parent_id) {
            if(++depth>store.size())return std::nullopt;
            record=store.find(*record->placement.parent_id);if(!record)return std::nullopt;
        }
        if(groupScope_==id)continue;
        const auto root=resolveHit(id,store);
        if(!root.isZero()&&std::find(resolved.begin(),resolved.end(),root)==resolved.end())resolved.push_back(root);
    }
    auto roots=resolved;
    roots.erase(std::remove_if(roots.begin(),roots.end(),[&](auto id) {
        const auto* record=store.find(id);
        while(record->placement.parent_id) {
            const auto parent=*record->placement.parent_id;
            if(std::find(resolved.begin(),resolved.end(),parent)!=resolved.end())return true;
            record=store.find(parent);
        }
        return false;
    }),roots.end());
    return roots;
}
bool SelectionSession::enterGroupScope(foundation::ObjectId id,const semantic::ObjectStore& store) {
    const auto* group=store.find(id);
    if(!group||group->kind!=semantic::ObjectKind::kGroup||resolveHit(id,store).isZero())return false;
    if(groupScope_!=id){groupScope_=id;++revision_;}cancel();return true;
}
void SelectionSession::leaveGroupScope(const semantic::ObjectStore& store) {
    auto parent=groupScope_;if(groupScope_){groupScope_.reset();++revision_;}cancel();
    while(parent) {
        const auto* r=store.find(*parent);if(!r)return;parent=r->placement.parent_id;
        if(parent)if(const auto* p=store.find(*parent);p&&p->kind==semantic::ObjectKind::kGroup){groupScope_=parent;++revision_;return;}
    }
}

bool SelectionSession::click(foundation::ObjectId objectId) {
    if (objectId.isZero()) return false;
    if(state_.selected_.size()!=1||state_.selected_[0]!=objectId)++revision_;
    state_.selected_ = {objectId};
    return true;
}

bool SelectionSession::toggle(foundation::ObjectId objectId) {
    if (objectId.isZero()) return false;
    ++revision_;
    const auto found = std::find(state_.selected_.begin(), state_.selected_.end(), objectId);
    if (found == state_.selected_.end()) {
        state_.selected_.push_back(objectId);
    } else {
        state_.selected_.erase(found);
    }
    return true;
}

bool SelectionSession::marquee(std::span<const HitCandidate> candidates) {
    auto next=hitTest_.selectable(candidates);
    if(next!=state_.selected_)++revision_;
    state_.selected_ = std::move(next);
    return true;
}

bool SelectionSession::applyAcquisition(std::span<const foundation::ObjectId> roots,
                                        SelectionCombineMode mode, std::uint64_t expectedRevision) {
    if (expectedRevision != revision_ ||
        (mode != SelectionCombineMode::kReplace && mode != SelectionCombineMode::kAdd) ||
        std::any_of(roots.begin(), roots.end(), [](auto id) { return id.isZero(); })) return false;
    auto next = mode == SelectionCombineMode::kAdd ? state_.selected_ : std::vector<foundation::ObjectId>{};
    for (auto id : roots) {
        if (std::find(next.begin(), next.end(), id) == next.end()) next.push_back(id);
    }
    if (next != state_.selected_) {
        state_.selected_ = std::move(next);
        ++revision_;
    }
    return true;
}

void SelectionSession::onChangeSet(const semantic::ChangeSet& changes) {
    bool changed=false;
    for (const semantic::ObjectSemanticChange& change : changes.objects()) {
        const auto flags = static_cast<std::uint8_t>(change.flags);
        const auto deleted = static_cast<std::uint8_t>(semantic::SemanticChangeFlags::kDeleted);
        if ((flags & deleted) == 0U) continue;
        if(groupScope_==change.object_id){groupScope_.reset();changed=true;}
        const auto before=state_.selected_.size();
        state_.selected_.erase(
            std::remove(state_.selected_.begin(), state_.selected_.end(), change.object_id),
            state_.selected_.end());
        changed=changed||before!=state_.selected_.size();
    }
    if(changed)++revision_;
}

void SelectionSession::cancel() noexcept { if(!state_.selected_.empty())++revision_;state_.selected_.clear(); }

} // namespace canvas::interaction
