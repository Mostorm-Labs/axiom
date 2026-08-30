#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"
#include <gtest/gtest.h>
#include <map>
#include <vector>

namespace canvas::semantic {
namespace {
class Applied final : public AppliedOperationView {
 public:
  std::optional<AppliedOperationEntry> find(const OperationId& id) const override { auto i=e.find(id); return i==e.end()?std::nullopt:std::optional<AppliedOperationEntry>(i->second); }
  std::map<OperationId, AppliedOperationEntry> e;
};
class Counting final : public ObjectStore {
 public:
  explicit Counting(const ObjectStore& s):s_(s){}
  std::size_t size() const noexcept override{return s_.size();} bool contains(const ObjectId&i) const noexcept override{return s_.contains(i);} const ObjectRecord* find(const ObjectId&i) const noexcept override{return s_.find(i);} std::vector<ObjectRecord> allObjects() const override{++all;return s_.allObjects();} std::vector<ObjectRecord> children(const std::optional<ObjectId>&i) const override{return s_.children(i);} mutable std::size_t all=0; private: const ObjectStore&s_;
};
Operation make(OperationPayload p, std::uint64_t n){ Operation o{}; o.id=OperationId{ObjectId::fromUint64(n)}; o.document_id=DocumentId{ObjectId::fromUint64(99)}; o.schema_version=1; o.payload_version=1; o.payload=std::move(p); return o; }
bool same(const PrepareResult&a,const PrepareResult&b){ if(a.disposition!=b.disposition||a.error.issue!=b.error.issue||a.plan.has_value()!=b.plan.has_value()) return false; if(!a.plan)return true; const auto&x=*a.plan; const auto&y=*b.plan; if(!(x.operation.id==y.operation.id)||x.operation.kind()!=y.operation.kind()||x.creates!=y.creates||x.replacements!=y.replacements||x.deletes!=y.deletes||x.delete_closure.has_value()!=y.delete_closure.has_value()) return false; if(x.delete_closure) return x.delete_closure->requested_delete_ids==y.delete_closure->requested_delete_ids&&x.delete_closure->resolved_hierarchy_closure==y.delete_closure->resolved_hierarchy_closure&&x.delete_closure->resolved_connector_cascade_closure==y.delete_closure->resolved_connector_cascade_closure&&x.delete_closure->final_delete_set==y.delete_closure->final_delete_set; return true; }
}
TEST(StatefulValidationDifferential, AllFifteenDispatchParityAndNoMutation){
 ReferenceObjectStore ref; IndexedObjectStore idx; Applied ar,ai; const std::vector<OperationPayload> ps={InsertObjectsOp{},DeleteObjectsOp{},RestoreObjectsOp{},SetPlacementsOp{},SetTransformsOp{},PatchPropertiesOp{},SetObjectSizeOp{},SetVectorPathGeometryOp{},SetImageContentOp{},AddStrokeOp{},SplitStrokesOp{},AddEraseMasksOp{},RemoveEraseMasksOp{},EditRichTextOp{},SetConnectorContentOp{}};
 for(std::size_t i=0;i<ps.size();++i){ auto o=make(ps[i],i+1); auto rb=ref.allObjects(), ib=idx.allObjects(); ASSERT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(idx)); Counting c(idx); auto rr=OperationEngine{}.prepare(o,StatefulValidationContext{ref,ar}); auto ir=OperationEngine{}.prepare(o,StatefulValidationContext{c,ai}); EXPECT_TRUE(same(rr,ir))<<i; if(i != 1U) EXPECT_EQ(c.all,0U)<<i; EXPECT_EQ(ref.allObjects(),rb); EXPECT_EQ(idx.allObjects(),ib); EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(idx)); }
}
TEST(StatefulValidationDifferential, AlreadyAppliedAndCollisionParity){ ReferenceObjectStore r; IndexedObjectStore i; Applied ar,ai; auto o=make(InsertObjectsOp{},77); ar.e.emplace(o.id,AppliedOperationEntry{o,std::nullopt}); ai.e.emplace(o.id,AppliedOperationEntry{o,std::nullopt}); auto a=OperationEngine{}.prepare(o,{r,ar}); auto b=OperationEngine{}.prepare(o,{i,ai}); EXPECT_TRUE(same(a,b)); auto c=o; c.payload=DeleteObjectsOp{}; auto d=OperationEngine{}.prepare(c,{r,ar}); auto e=OperationEngine{}.prepare(c,{i,ai}); EXPECT_TRUE(same(d,e)); EXPECT_EQ(d.error.issue,StatefulIssue::kOperationIdCollision); }
}
