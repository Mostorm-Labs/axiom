#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
namespace canvas::debug_ui {
enum class SurfaceMode : std::uint8_t { kPlatformDefault, kCpuReference, kGpuDefault };
struct SurfaceReceipt final { std::uint64_t requestId=0; std::uint64_t generation=0; SurfaceMode mode=SurfaceMode::kPlatformDefault; bool accepted=false; };
class PlatformDebugControl final { public: explicit PlatformDebugControl(SurfaceMode mode=SurfaceMode::kPlatformDefault): mode_(mode) {} [[nodiscard]] SurfaceReceipt requestSurfaceMode(std::uint64_t id, std::uint64_t expectedGeneration, SurfaceMode mode){ std::lock_guard lock(mutex_); if(expectedGeneration!=generation_) return {id,generation_,mode_,false}; mode_=mode; ++generation_; return {id,generation_,mode_,true}; } [[nodiscard]] std::uint64_t generation() const { std::lock_guard lock(mutex_); return generation_; } [[nodiscard]] SurfaceMode mode() const { std::lock_guard lock(mutex_); return mode_; } private: mutable std::mutex mutex_; std::uint64_t generation_=1; SurfaceMode mode_;};
} // namespace canvas::debug_ui
