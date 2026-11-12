#pragma once
#include "cpu_exact_path_kernel.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#ifndef AXIOM_CPU_KERNEL_ID
#define AXIOM_CPU_KERNEL_ID "00000000000000000000000000000000"
#endif
namespace canvas::render::internal {
constexpr std::size_t cpuInputLimit=1048576,cpuOutputLimit=1048576,cpuHeaderSize=128;
constexpr std::size_t cpuSweepLimit=8192,cpuFilledLimit=1900,cpuHeapBytes=64U*1024U*1024U;
// Checked task construction lives in the charged fixed Worker heap, not the
// separate8MiB resident path cache. Includes two conservative path/COW sides,
// decoded values, input/result copies, stack/runtime reserve.
std::optional<std::size_t> cpuConstructionEstimate(std::size_t count,std::size_t wireBytes,bool sweep)noexcept;
struct CpuIdentity {
 std::array<std::uint64_t,8> words{}; // runtime,lifetime,doc0,doc1,executor,job,attempt,source
 bool operator==(const CpuIdentity&)const=default;
};
struct CpuRequest {CpuIdentity identity;semantic::EraseMaskGeometry geometry;};
std::array<std::uint8_t,16> cpuKernelId();
std::vector<std::uint8_t> cpuEncodeRequest(const CpuRequest&);
std::optional<CpuRequest> cpuDecodeRequest(std::span<const std::uint8_t>);
std::optional<SkPath> cpuDecodePath(std::span<const std::uint8_t>);
}
