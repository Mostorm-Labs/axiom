#pragma once
#include "canvas/input/pointer_sample_batch.hpp"
#include <cstddef>
#include <cstdint>
namespace canvas::input { enum class DispatchDisposition : std::uint8_t { kDelivered, kCancelled, kRejected }; class InputRouter final { public: DispatchDisposition dispatch(const PointerSampleBatch&); std::size_t confirmedCount() const noexcept{return confirmed_;} std::size_t predictedCount() const noexcept{return predicted_;} std::uint64_t lastConfirmedSequence() const noexcept{return lastConfirmed_;} private: std::size_t confirmed_=0; std::size_t predicted_=0; std::uint64_t lastConfirmed_=0; }; }
