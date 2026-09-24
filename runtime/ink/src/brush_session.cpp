#include "canvas/ink/brush_session.hpp"
#include <limits>
#include <cmath>
#include <limits>

namespace canvas::ink {
namespace {
bool validSample(const BrushSample& sample) {
    if (!std::isfinite(sample.x) || !std::isfinite(sample.y) ||
        (sample.pressurePresent && (!std::isfinite(sample.pressure) || sample.pressure < 0.0 || sample.pressure > 1.0))) return false;
    return true;
}
}

BrushSession::BrushSession(std::uint64_t id, ResolvedBrushState state) : id_(id), state_(std::move(state)) {}
bool BrushSession::begin() { if (active_ || id_ == 0) return false; active_ = true; return true; }
BrushSessionError BrushSession::append(std::span<const BrushSample> confirmed,
                                       std::span<const BrushSample> predicted,
                                       BrushPreviewDelta& out) {
    if (!active_) { error_ = BrushSessionError::kInvalid; return error_; }
    auto sequence = lastSequence_;
    for (const auto& sample : confirmed) {
        if (!validSample(sample)) { error_ = BrushSessionError::kInvalid; return error_; }
        if (sample.sequence == 0 || sample.sequence <= sequence) { error_ = BrushSessionError::kSequence; return error_; }
        if (state_.package.vector.pressureSource == BrushPressureSource::kDevice &&
            !sample.pressurePresent && state_.package.vector.missingPressure == BrushMissingPressure::kReject) {
            error_ = BrushSessionError::kPressure; return error_;
        }
        sequence = sample.sequence;
    }
    for (const auto& sample : predicted) {
        if (!validSample(sample)) { error_ = BrushSessionError::kInvalid; return error_; }
    }

    // Evaluate a candidate state first. A failed reference evaluation must not
    // publish a revision or partially append confirmed samples.
    auto candidate = confirmed_;
    candidate.insert(candidate.end(), confirmed.begin(), confirmed.end());
    std::vector<reference::VectorStrokeInput> input;
    input.reserve(candidate.size() + predicted.size());
    for (const auto& sample : candidate) input.push_back({sample.x, sample.y, sample.pressurePresent ? sample.pressure : std::numeric_limits<double>::quiet_NaN()});
    for (const auto& sample : predicted) input.push_back({sample.x, sample.y, sample.pressurePresent ? sample.pressure : std::numeric_limits<double>::quiet_NaN()});
    auto result = VectorPathNode(state_).evaluate(input, false);
    if (!result) { error_ = BrushSessionError::kInvalid; return error_; }
    confirmed_ = std::move(candidate);
    if (!confirmed.empty()) lastSequence_ = confirmed.back().sequence;
    out = {++revision_, std::move(result.outline)};
    return error_ = BrushSessionError::kNone;
}
BrushSessionError BrushSession::seal(BrushCommitIntent& out) {
    if (!active_ || confirmed_.empty()) { error_ = BrushSessionError::kEmpty; return error_; }
    std::vector<reference::VectorStrokeInput> input;
    for (const auto& sample : confirmed_) input.push_back({sample.x, sample.y, sample.pressurePresent ? sample.pressure : std::numeric_limits<double>::quiet_NaN()});
    auto result = VectorPathNode(state_).evaluate(input, true);
    if (!result) { error_ = BrushSessionError::kEmpty; return error_; }
    out = {id_, ++revision_, state_.seed, confirmed_, std::move(result.outline)};
    active_ = false;
    return error_ = BrushSessionError::kNone;
}
void BrushSession::cancel() noexcept { active_ = false; confirmed_.clear(); }
}
