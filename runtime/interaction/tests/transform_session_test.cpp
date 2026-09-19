#include "canvas/interaction/transform_session.hpp"
#include <array>
#include <cassert>
using namespace canvas;
namespace {
struct Submit final : interaction::TransformSubmitPort {
 interaction::SubmitResult submit(const semantic::SetTransformsOp& op) override { ++calls; last=op; return interaction::SubmitResult::acceptedResult(); }
 int calls=0; semantic::SetTransformsOp last;
};
foundation::ObjectId id(std::uint64_t v){return foundation::ObjectId::fromUint64(v);}
void preview_zero_ops_commit_exact_one(){
 Submit s; interaction::TransientSceneOverride o; interaction::TransformSession x(s,o);
 semantic::Transform2D a; a.tx=1; semantic::Transform2D b; b.ty=2;
 const std::array<std::pair<foundation::ObjectId,semantic::Transform2D>,2> targets{{{id(1),{}},{id(2),{}}}};
 const std::array<std::pair<foundation::ObjectId,semantic::Transform2D>,2> values{{{id(1),a},{id(2),b}}};
 assert(x.begin(targets));
 assert(x.preview(values)); assert(s.calls==0); assert(o.size()==2);
 assert(x.commit()); assert(s.calls==1); assert(s.last.items.size()==2); assert(s.last.items[0].transform==a); assert(o.size()==0);
}
void cancel_and_deleted_target_never_submit(){
 Submit s; interaction::TransientSceneOverride o; interaction::TransformSession x(s,o);
 const std::array<std::pair<foundation::ObjectId,semantic::Transform2D>,1> target{{{id(3),{}}}};
 assert(x.begin(target)); x.cancel(); assert(s.calls==0);
 assert(x.begin(target));
 auto c=semantic::ChangeSet::fromChanges(semantic::SemanticGeneration(1),semantic::SemanticGeneration(2),{{id(3),semantic::SemanticChangeFlags::kDeleted,{}}});
 assert(x.onChangeSet(c)==interaction::TransformConflict::kCancel); assert(!x.commit()); assert(s.calls==0);
}
}
int main(){preview_zero_ops_commit_exact_one();cancel_and_deleted_target_never_submit();}
