#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace canvas::debug_ui {
struct TelemetrySample final { std::uint64_t sequence=0; std::uint64_t inputEvents=0; std::uint64_t previewFrames=0; std::uint64_t canonicalFrames=0; double inputToPreviewMs=0.0; };
class RollingTelemetry final { public: explicit RollingTelemetry(std::size_t capacity=120): capacity_(capacity) {} void push(TelemetrySample sample){ if(capacity_==0)return; if(samples_.size()==capacity_) samples_.erase(samples_.begin()); samples_.push_back(sample);} [[nodiscard]] std::vector<TelemetrySample> read() const { return samples_; } [[nodiscard]] std::size_t size() const noexcept{return samples_.size();} private: std::size_t capacity_; std::vector<TelemetrySample> samples_; };
enum class TraceState : std::uint8_t { kDisabled, kRecording, kSealed, kOverflow, kFailed };
class BoundedTraceSession final { public: explicit BoundedTraceSession(std::size_t budget): budget_(budget) {} bool start(){ if(budget_==0)return false; state_=TraceState::kRecording; used_=0; return true;} bool append(std::size_t bytes){ if(state_!=TraceState::kRecording)return false; if(bytes>budget_-used_){state_=TraceState::kOverflow; return false;} used_+=bytes; return true;} void stop(){if(state_==TraceState::kRecording)state_=TraceState::kSealed;} [[nodiscard]] TraceState state()const noexcept{return state_;} [[nodiscard]] std::size_t used()const noexcept{return used_;} private: std::size_t budget_=0, used_=0; TraceState state_=TraceState::kDisabled;};
} // namespace canvas::debug_ui
