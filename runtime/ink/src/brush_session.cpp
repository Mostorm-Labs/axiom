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

std::optional<BrushSample> BrushSession::normalize(const BrushSample& sample) const {
    if (!validSample(sample)) return std::nullopt;
    BrushSample normalized = sample;
    if (state_.package.vector.pressureSource == BrushPressureSource::kSimulated) {
        normalized.pressure = 0.0;
        normalized.pressurePresent = false;
        return normalized;
    }
    if (!normalized.pressurePresent) {
        if (state_.package.vector.missingPressure == BrushMissingPressure::kReject)
            return std::nullopt;
        normalized.pressure = 0.5;
        normalized.pressurePresent = true;
    }
    return normalized;
}

BrushSession::BrushSession(std::uint64_t id, ResolvedBrushState state) : id_(id), state_(std::move(state)) {}
bool BrushSession::begin() { if (active_ || id_ == 0) return false; active_ = true; return true; }
BrushSessionError BrushSession::append(std::span<const BrushSample> confirmed,
                                       std::span<const BrushSample> predicted,
                                       BrushPreviewDelta& out) {
    if (!active_) { error_ = BrushSessionError::kInvalid; return error_; }
    auto sequence = lastSequence_;
    std::vector<BrushSample> normalizedConfirmed;
    normalizedConfirmed.reserve(confirmed.size());
    for (const auto& sample : confirmed) {
        const auto normalized = normalize(sample);
        if (!normalized) { error_ = state_.package.vector.pressureSource == BrushPressureSource::kDevice &&
                                   state_.package.vector.missingPressure == BrushMissingPressure::kReject
                               ? BrushSessionError::kPressure : BrushSessionError::kInvalid; return error_; }
        if (sample.sequence == 0 || sample.sequence <= sequence) { error_ = BrushSessionError::kSequence; return error_; }
        sequence = sample.sequence;
        normalizedConfirmed.push_back(*normalized);
    }
    std::vector<BrushSample> normalizedPredicted;
    normalizedPredicted.reserve(predicted.size());
    for (const auto& sample : predicted) {
        const auto normalized = normalize(sample);
        if (!normalized) { error_ = BrushSessionError::kInvalid; return error_; }
        normalizedPredicted.push_back(*normalized);
    }

    // Preview publishes the complete retained geometry.  The outline is not
    // safely concatenable across tails because smoothing, thinning and caps
    // can change earlier vertices as new samples arrive.  Re-evaluating the
    // normalized confirmed history keeps preview, seal and replay on one
    // geometry authority; predicted samples remain preview-only.
    const auto oldSize = confirmed_.size();
    confirmed_.insert(confirmed_.end(), normalizedConfirmed.begin(), normalizedConfirmed.end());
    std::vector<reference::VectorStrokeInput> input;
    input.reserve(confirmed_.size() + normalizedPredicted.size());
    for (std::size_t i = 0; i < confirmed_.size(); ++i) {
        const auto& sample = confirmed_[i];
        input.push_back({sample.x, sample.y, sample.pressurePresent ? sample.pressure : std::numeric_limits<double>::quiet_NaN()});
    }
    for (const auto& sample : normalizedPredicted) input.push_back({sample.x, sample.y, sample.pressurePresent ? sample.pressure : std::numeric_limits<double>::quiet_NaN()});
    auto result = VectorPathNode(state_).evaluate(input, false);
    metrics_.appendEvaluations += 1;
    metrics_.maxAppendInputSamples = std::max<std::uint64_t>(metrics_.maxAppendInputSamples, input.size());
    metrics_.maxCopiedHistoricalSamples = std::max<std::uint64_t>(metrics_.maxCopiedHistoricalSamples, confirmed_.size());
    if (!result) { confirmed_.resize(oldSize); error_ = BrushSessionError::kInvalid; return error_; }
    if (!confirmed.empty()) lastSequence_ = confirmed.back().sequence;
    out = {++revision_, std::move(result.outline)};
    return error_ = BrushSessionError::kNone;
}
BrushSessionError BrushSession::seal(BrushCommitIntent& out) {
    if (!active_ || confirmed_.empty()) { error_ = BrushSessionError::kEmpty; return error_; }
    std::vector<reference::VectorStrokeInput> input;
    for (const auto& sample : confirmed_) input.push_back({sample.x, sample.y, sample.pressurePresent ? sample.pressure : std::numeric_limits<double>::quiet_NaN()});
    auto result = VectorPathNode(state_).evaluate(input, true);
    metrics_.sealEvaluations += 1;
    if (!result) { error_ = BrushSessionError::kEmpty; return error_; }
    out = {id_, ++revision_, state_.seed, confirmed_, std::move(result.outline)};
    active_ = false;
    return error_ = BrushSessionError::kNone;
}
void BrushSession::cancel() noexcept { active_ = false; confirmed_.clear(); }
}
