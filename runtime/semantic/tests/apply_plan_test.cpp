#include "canvas/semantic/apply_plan.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include <gtest/gtest.h>
#include <map>

namespace canvas::semantic {
namespace {
class Applied final : public AppliedOperationView {
 public:
  std::optional<AppliedOperationEntry> find(const OperationId& id) const override {
    auto it = entries.find(id); return it == entries.end() ? std::nullopt : std::optional<AppliedOperationEntry>(it->second);
  }
  std::map<OperationId, AppliedOperationEntry> entries;
};
class EmptyStore final : public ObjectStore {
 public:
  std::size_t size() const noexcept override { return 0; }
  bool contains(const ObjectId&) const noexcept override { return false; }
  const ObjectRecord* find(const ObjectId&) const noexcept override { return nullptr; }
  std::vector<ObjectRecord> allObjects() const override { return {}; }
  std::vector<ObjectRecord> children(const std::optional<ObjectId>&) const override { return {}; }
};
Operation op() { Operation o{}; o.schema_version=1; o.payload_version=1; o.payload=InsertObjectsOp{}; return o; }
}
TEST(ApplyPlan, AlreadyAppliedHasNoPlan) {
  EmptyStore store; Applied applied; auto o=op(); applied.entries.emplace(o.id, AppliedOperationEntry{o,std::nullopt});
  auto r=prepareApplyPlan(o, StatefulValidationContext{store,applied});
  EXPECT_EQ(r.disposition, PrepareDisposition::kAlreadyApplied); EXPECT_FALSE(r.plan.has_value()); EXPECT_TRUE(r.error.ok());
}
TEST(ApplyPlan, NewInsertProducesPreparedPlan) {
  EmptyStore store; Applied applied; auto r=prepareApplyPlan(op(), StatefulValidationContext{store,applied});
  EXPECT_EQ(r.disposition, PrepareDisposition::kPrepared); ASSERT_TRUE(r.plan.has_value()); EXPECT_TRUE(r.error.ok());
}
TEST(ApplyPlan, CollisionRejectsWithoutPlan) {
  EmptyStore store; Applied applied; auto existing=op(); applied.entries.emplace(existing.id, AppliedOperationEntry{existing,std::nullopt}); auto incoming=existing; incoming.payload=DeleteObjectsOp{};
  auto r=prepareApplyPlan(incoming, StatefulValidationContext{store,applied});
  EXPECT_EQ(r.disposition, PrepareDisposition::kRejected); EXPECT_EQ(r.error.issue, StatefulIssue::kOperationIdCollision); EXPECT_FALSE(r.plan.has_value());
}
}
