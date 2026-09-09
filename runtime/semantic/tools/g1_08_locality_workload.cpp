#include "g1_08_locality_workload.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "object_store_mutator.hpp"
#include <cstdint>
#include <optional>

namespace canvas::verification::g1_08 {
namespace {
using namespace canvas::semantic;
ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt) {
    ObjectRecord r{}; r.id=ObjectId::fromUint64(value); r.kind=ObjectKind::kShape; r.kind_version=1U;
    r.placement=Placement{parent,OrderKey({static_cast<std::uint8_t>((value%250U)+1U)})};
    r.transform=Transform2D{}; r.content=ShapeContent{1U,1.0,1.0}; return r;
}
ObjectRecord group(std::uint64_t value) { auto r=shape(value); r.kind=ObjectKind::kGroup; r.content=GroupContent{}; return r; }
ObjectRecord connector(std::uint64_t value,ObjectId target) { auto r=shape(value); r.kind=ObjectKind::kConnector; ConnectorContent c{}; c.start.value=AttachedEndpoint{target,AutoPerimeterAnchor{}}; c.end.value=FreePointEndpoint{Vec2{1.0,1.0}}; c.routing=ConnectorRouting::kStraight; r.content=c; return r; }
void populateCold(IndexedObjectStore& s,std::size_t n,std::uint64_t first){for(std::size_t i=0;i<n;++i) static_cast<void>(internal::ObjectStoreMutator::insertFresh(s,shape(first+i)));}
template<class Payload> LocalityWorkloadResult applyWorkload(const char* name,std::size_t total,std::size_t affected,IndexedObjectStore& s,Payload payload,std::uint64_t oid){
    AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(42U)); Operation o{}; o.id=OperationId{ObjectId::fromUint64(oid)}; o.document_id=DocumentId{ObjectId::fromUint64(99U)}; o.schema_version=1U; o.payload_version=1U; o.payload=std::move(payload);
    internal::resetIndexedAccessProbe(); internal::enableIndexedAccessProbe(true); const auto applied=OperationEngine{}.apply(o,ApplySource::kLocalInteraction,s,l,g,c); const auto access=internal::snapshotIndexedAccessProbe(); internal::enableIndexedAccessProbe(false); return {name,total,affected,applied.disposition==ApplyDisposition::kApplied,access};
}
}
LocalityWorkloadResult runIndexedLocalityWorkload(std::size_t total){IndexedObjectStore s;for(std::uint64_t i=1;i<=8;++i) static_cast<void>(internal::ObjectStoreMutator::insertFresh(s,shape(i)));populateCold(s,total-8U,1000U);SetTransformsOp p;for(std::uint64_t i=1;i<=8;++i)p.items.push_back({ObjectId::fromUint64(i),Transform2D{1,0,0,1,static_cast<double>(i),2}});return applyWorkload("W08-LOCAL-MUTATION",total,8U,s,std::move(p),8001);}
LocalityWorkloadResult runHierarchyWorkload(std::size_t total){IndexedObjectStore s;static_cast<void>(internal::ObjectStoreMutator::insertFresh(s,group(1)));for(std::uint64_t i=2;i<=8;++i)static_cast<void>(internal::ObjectStoreMutator::insertFresh(s,shape(i,ObjectId::fromUint64(1))));populateCold(s,total-8U,1000U);SetPlacementsOp p;for(std::uint64_t i=2;i<=8;++i)p.items.push_back({ObjectId::fromUint64(i),Placement{ObjectId::fromUint64(1),OrderKey({static_cast<std::uint8_t>(i)})}});return applyWorkload("W08-HIERARCHY",total,8U,s,std::move(p),8002);}
LocalityWorkloadResult runConnectorDeleteWorkload(std::size_t total){IndexedObjectStore s;static_cast<void>(internal::ObjectStoreMutator::insertFresh(s,shape(1)));for(std::uint64_t i=0;i<7;++i)static_cast<void>(internal::ObjectStoreMutator::insertFresh(s,connector(100+i,ObjectId::fromUint64(1))));populateCold(s,total-8U,1000U);return applyWorkload("W08-CONNECTOR-DELETE",total,8U,s,DeleteObjectsOp{{ObjectId::fromUint64(1)}},8003);}
LocalityWorkloadResult runControlledCascadeWorkload(std::size_t n){constexpr std::size_t total=100000U;IndexedObjectStore s;static_cast<void>(internal::ObjectStoreMutator::insertFresh(s,shape(1)));for(std::size_t i=0;i<n;++i)static_cast<void>(internal::ObjectStoreMutator::insertFresh(s,connector(100+i,ObjectId::fromUint64(1))));populateCold(s,total-n-1U,1000000U);return applyWorkload("W08-CONTROLLED-CASCADE",total,n+1U,s,DeleteObjectsOp{{ObjectId::fromUint64(1)}},8100+n);}
}
