#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "canvas/interaction/internal/cpu_pending_erase.hpp"

namespace canvas::semantic { struct Operation; }
namespace canvas::runtime {
class RuntimeComposition;
class RuntimeCompositionCore;

// Transitional internal bridge for platform migration. It never owns a core.
class RuntimePlatformAccess final {
 public:
  [[nodiscard]] static RuntimeCompositionCore& core(RuntimeComposition&) noexcept;
  using PreparedEncodedPublication=bool(*)(void*,const semantic::Operation&,std::span<const std::uint8_t>)noexcept;
  static bool setPreparedEncodedPublication(RuntimeComposition&,PreparedEncodedPublication)noexcept;
  static void failCpuPendingExecutor(RuntimeComposition&)noexcept;
  static void suspendCpuPendingExecutor(RuntimeComposition&)noexcept;
  static bool bindCpuPendingExecutor(RuntimeComposition&,std::uint64_t)noexcept;
  static std::vector<std::uint8_t> takeCpuPendingRequest(RuntimeComposition&)noexcept;
  static bool queueCpuPendingCompletion(RuntimeComposition&,std::array<std::uint64_t,8>,std::span<const std::uint8_t>,bool)noexcept;
  static std::array<std::uint64_t,4> cpuWorkerBasis(RuntimeComposition&)noexcept;
  static interaction::PendingLocalValue pendingLocalValue(RuntimeComposition&);
  static interaction::CpuPendingEraseFacts cpuPendingFacts(RuntimeComposition&)noexcept;
  static std::uint64_t reserveCpuWorkerGrant(RuntimeComposition&,std::uint64_t,std::size_t)noexcept;
  static bool releaseCpuWorkerGrant(RuntimeComposition&,std::uint64_t,std::uint64_t)noexcept;
  static std::size_t cpuWorkerGrantBytes(RuntimeComposition&)noexcept;
};
}  // namespace canvas::runtime
