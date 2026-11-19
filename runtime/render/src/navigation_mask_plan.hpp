#pragma once
#include "skia_mask_history.hpp"
#include "mask_primitive_observation.hpp"
#include "include/core/SkPathBuilder.h"
#include <list>
namespace canvas::render::internal {
struct NavigationMaskUsage {
 std::size_t bytes=0,capacity=0;
 std::shared_ptr<MaskHistoryUsage> owner;
 std::size_t used()const noexcept{return owner?owner->bytes:bytes;}
};
struct NavigationMaskCharge {
 std::shared_ptr<NavigationMaskUsage> usage;std::size_t bytes=0;
 ~NavigationMaskCharge(){MPS_IF(mps::uncharge(this));if(usage){usage->bytes-=bytes;if(usage->owner)usage->owner->bytes-=bytes;}}
};
struct NavigationMaskIdentity {
 ImmutableSourceIdentity source;std::shared_ptr<resources::ResourceBudgetManager> budget;
};
struct NavigationMaskIdentityLess {
 bool operator()(const NavigationMaskIdentity&a,const NavigationMaskIdentity&b)const noexcept{
  if(a.budget.owner_before(b.budget))return true;if(b.budget.owner_before(a.budget))return false;
  return ImmutableSourceIdentityLess{}(a.source,b.source);
 }
};
struct NavigationMaskBound {std::size_t record=0,primitive=0;SkRect bounds{};bool filled=false;};
struct NavigationMaskPage {std::array<NavigationMaskBound,64> items{};std::size_t size=0;SkRect bounds{};resources::ResourceLease lease;std::shared_ptr<NavigationMaskCharge> charge;};
struct NavigationMaskPlan {
 scene::SharedVector<semantic::EraseMaskRecord> records;
 std::shared_ptr<resources::ResourceBudgetManager> budget;
 std::shared_ptr<SourceRetentionPool> pool;
 std::shared_ptr<const SourceRetention> source;
 resources::ResourceLease metadata;
 std::list<resources::ResourceLease> accounting;
 std::shared_ptr<NavigationMaskUsage> usage;
 std::shared_ptr<NavigationMaskCharge> charge;
 std::map<std::size_t,std::shared_ptr<NavigationMaskPage>> pages;
 struct Cached {std::shared_ptr<const PreparedMaskPrimitive> primitive;std::uint64_t used=0;std::shared_ptr<NavigationMaskCharge> charge;};
 std::map<std::pair<std::size_t,std::size_t>,Cached> paths;
 std::size_t record=0,primitive=0,commands=0,units=0,footprint=0,indexBytes=0,pathBytes=0,capacity=0;
 std::uint64_t tick=0;bool header=false,ready=false,invalid=false,streaming=false;
 SkRect bounds{},filledBounds{};bool hasFilledBounds=false;
 resources::ResourceLease indexMetadata;
 bool lastWasMove=false; // Kept only for finite path validation; no semantic reinterpretation.
};
struct NavigationMaskReplay {
 std::size_t ordinal=0,record=0,primitive=0,filledCommand=0,lastDraws=0;bool initialized=false;
 // Scratch belongs to exactly one immutable history and primitive. It may
 // be superseded by another job publishing the shared prepared path.
 std::shared_ptr<const NavigationMaskPlan> buildingPlan;
 std::size_t buildingOrdinal=0;
 std::unique_ptr<SkPathBuilder> filled;
 resources::ResourceLease staging;
 std::shared_ptr<const PreparedMaskPrimitive> ready;
};
}
