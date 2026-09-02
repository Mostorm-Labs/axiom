#include "canvas/semantic/operation_engine.hpp"
#include <gtest/gtest.h>
namespace canvas::semantic {
namespace { class S final : public ObjectStore { public: std::size_t size() const noexcept override{return 0;} bool contains(const ObjectId&) const noexcept override{return false;} const ObjectRecord* find(const ObjectId&) const noexcept override{return nullptr;} std::vector<ObjectRecord> allObjects() const override{return{};} std::vector<ObjectRecord> children(const std::optional<ObjectId>&) const override{return{};} }; class A final: public AppliedOperationView { public: std::optional<AppliedOperationEntry> find(const OperationId&) const override{return std::nullopt;} }; }
TEST(OperationEngineBoundary, DelegatesToPlanner) { S s; A a; Operation o{}; o.schema_version=1; o.payload_version=1; o.payload=InsertObjectsOp{}; OperationEngine e; auto r=e.prepare(o, StatefulValidationContext{s,a}); EXPECT_EQ(r.disposition, PrepareDisposition::kPrepared); }
}
