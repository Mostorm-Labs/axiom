#include "canvas/interaction/eraser_session.hpp"
#include <array>
#include <cassert>
using namespace canvas;
namespace {
struct Submit final : interaction::EraseSubmitPort { interaction::SubmitResult submit(const semantic::DeleteObjectsOp& op) override {++calls;last=op;return interaction::SubmitResult::acceptedResult();} int calls=0; semantic::DeleteObjectsOp last;};
foundation::ObjectId id(std::uint64_t v){return foundation::ObjectId::fromUint64(v);}
void mixed_candidates_only_delete_ink(){
 Submit s; interaction::EraserSession e(s);
 const std::array<interaction::EraseCandidate,7> c{{
  {id(1),semantic::ObjectKind::kVectorStroke,true,false},{id(2),semantic::ObjectKind::kDabStroke,true,false},
  {id(3),semantic::ObjectKind::kShape,true,false},{id(4),semantic::ObjectKind::kRichText,true,false},
  {id(5),semantic::ObjectKind::kImage,true,false},{id(6),semantic::ObjectKind::kVectorStroke,false,false},
  {id(7),semantic::ObjectKind::kDabStroke,true,true}}};
 assert(e.begin()); assert(e.sweep(c)); assert(e.commit()); assert(s.calls==1); assert(s.last.object_ids==std::vector<foundation::ObjectId>({id(1),id(2)}));
}
void cancel_and_empty_submit_nothing(){
 Submit s; interaction::EraserSession e(s); assert(e.begin()); e.cancel(); assert(!e.commit());
 const std::array<interaction::EraseCandidate,1> c{{{id(8),semantic::ObjectKind::kConnector,true,false}}}; assert(e.begin()); assert(e.sweep(c)); assert(!e.commit()); assert(s.calls==0);
}
}
int main(){mixed_candidates_only_delete_ink();cancel_and_empty_submit_nothing();}
