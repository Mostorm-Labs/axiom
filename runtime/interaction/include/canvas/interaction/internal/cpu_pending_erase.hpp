#pragma once
// Private owner-thread transport values, never installed consumer capabilities.
#include "canvas/foundation/object_id.hpp"
#include <optional>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace canvas::interaction {
struct CpuPendingEraseTerminal final {
 std::uint64_t serial=0,session=0,jobs=0,operationOrdinal=0;
 std::array<std::uint64_t,8> token{};
 std::string status;
 std::uint64_t generation=0;
};
struct CpuPendingJobFact final {
 std::array<std::uint64_t,8> token{};
 bool historical=false;std::size_t inputBytes=0,outputBytes=0;
 std::uint64_t issuedNs=0,returnedNs=0,importNs=0;
 std::string status;
};
struct PendingLocalIntentValue final {
 std::array<std::uint64_t,3> token{};std::string kind,phase;
 std::vector<canvas::foundation::ObjectId> objects;std::vector<std::uint64_t> dependencies;
};
struct PendingLocalValue final {
 std::array<std::uint64_t,2> owner{};
 std::uint64_t revision=0;std::vector<PendingLocalIntentValue> intents;
 std::vector<canvas::foundation::ObjectId> selected;std::optional<std::array<float,4>> bounds;
};
struct CpuPendingEraseFacts final {
 bool capability=false,enabled=false,requestReady=false,suspended=false,historyJob=false;
 std::size_t pending=0,reserved=0;
 std::uint64_t head=0,job=0,admitted=0,committed=0,noops=0,failed=0;
 std::string state,error;
 std::array<std::uint64_t,8> token{};
 std::vector<CpuPendingEraseTerminal> terminals;
 std::vector<CpuPendingJobFact> jobLedger;
 std::uint64_t newQueryUnions=0,historyUnions=0,retainedPathBytes=0,finishedImports=0,finishedHits=0;
 std::uint64_t prepareNs=0,importNs=0,exactNs=0,commitNs=0,workerWaitNs=0;
 std::uint64_t targetPrepareNs=0,sceneStampPrepareNs=0,historyPrepareNs=0,publicationPrepareNs=0,publishNs=0,maximumCommitStageNs=0;
 std::uint64_t jobs=0,geometryReuse=0,exactCalls=0,opportunities=0,maxOwnerNs=0,maxExactNs=0;
 std::uint64_t preparedBeforeUp=0,suffixAfterUp=0,checkpointBytes=0,allUnionSteps=0;
};
struct CpuPendingEraseState;
}
