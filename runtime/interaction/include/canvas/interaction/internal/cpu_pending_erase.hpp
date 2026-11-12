#pragma once
// Private owner-thread transport values, never installed consumer capabilities.
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace canvas::interaction {
struct CpuPendingEraseTerminal final {
 std::uint64_t serial=0,session=0,jobs=0,operationOrdinal=0;
 std::array<std::uint64_t,8> token{};
 std::string status;
};
struct CpuPendingJobFact final {
 std::array<std::uint64_t,8> token{};
 bool historical=false;std::size_t inputBytes=0,outputBytes=0;
 std::uint64_t issuedNs=0,returnedNs=0,importNs=0;
 std::string status;
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
 std::uint64_t jobs=0,geometryReuse=0,exactCalls=0,opportunities=0,maxOwnerNs=0,maxExactNs=0;
};
struct CpuPendingEraseState;
}
