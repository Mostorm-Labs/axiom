#include "canvas/ink/chalk_dab_evaluator.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace canvas::ink {

std::uint64_t ChalkDabEvaluator::digest(std::span<const BrushDab> dabs, std::uint64_t seed) {
    std::uint64_t hash = 1469598103934665603ULL ^ seed;
    for (const auto& dab : dabs) {
        const auto mix = [&hash](double value) {
            std::uint64_t bits = 0;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            for (unsigned i = 0; i < 8; ++i) { hash ^= (bits >> (i * 8U)) & 0xffU; hash *= 1099511628211ULL; }
        };
        mix(dab.x); mix(dab.y); mix(dab.size); mix(dab.rotation); mix(dab.opacity);
        hash ^= dab.materialRevision; hash *= 1099511628211ULL;
        if (dab.materialMode != 0U) {
            hash ^= dab.materialMode;
            hash *= 1099511628211ULL;
        }
    }
    return hash == 0 ? 1 : hash;
}

std::vector<BrushDab> ChalkDabEvaluator::evaluate(
    const ResolvedBrushState& state,
    std::span<const reference::VectorStrokeInput> samples) {
    std::vector<BrushDab> result;
    if (state.package.profileId != "chalk-grain-v1" ||
        (state.package.revision < 2U || state.package.revision > 4U) || samples.empty()) return result;
    reference::StrokeOptions options;
    const auto& v = state.package.vector;
    options.size = v.size;
    options.thinning = v.thinning;
    options.smoothing = v.smoothing;
    options.streamline = v.streamline;
    options.simulate_pressure = v.pressureSource == BrushPressureSource::kSimulated;
    options.start_cap = v.startCap;
    options.end_cap = v.endCap;
    // The last raw input determines the complete path's endpoint. This is
    // independent of whether the outline preview has a predicted tail.
    options.last = true;
    const auto points = reference::getStrokePoints(samples, options);
    if (points.empty()) return result;
    const double step = std::max(0.5, v.size * state.package.grain.spacing * 0.1);
    const double total = points.back().running_length;
    const std::size_t count = total <= 0.0 ? 1U : static_cast<std::size_t>(std::floor(total / step)) + 1U;
    result.reserve(count);
    std::size_t segment = 1U;
    for (std::size_t index = 0; index < count; ++index) {
        const double distance = total <= 0.0 ? 0.0 : std::min(total, static_cast<double>(index) * step);
        while (segment < points.size() && points[segment].running_length < distance) ++segment;
        const auto& right = points[std::min(segment, points.size() - 1U)];
        const auto& left = points[segment == 0U ? 0U : segment - 1U];
        const double span = right.running_length - left.running_length;
        const double t = span > 0.0 ? (distance - left.running_length) / span : 0.0;
        const auto lerp = [t](double a, double b) { return a + (b - a) * t; };
        const double x = lerp(left.x, right.x);
        const double y = lerp(left.y, right.y);
        const double rotation = std::atan2(right.y - left.y, right.x - left.x);
        const double pressure = lerp(left.pressure, right.pressure);
        const double radius = v.size * (0.5 - v.thinning * (0.5 - pressure));
        const float jitter = 0.9F + 0.2F * deterministicChannel(state.seed, RandomChannel::kSize, index);
        const float opacityJitter = 0.9F + 0.1F * deterministicChannel(state.seed, RandomChannel::kOpacity, index);
        result.push_back({x, y, std::max(0.01, radius * 2.0 * jitter), static_cast<float>(rotation),
                          static_cast<float>(std::clamp(state.package.paint.opacity * state.package.grain.opacity * opacityJitter, 0.0, 1.0)),
                          state.package.revision});
    }
    return result;
}

std::vector<BrushDab> MembraneDabEvaluator::evaluate(
    const ResolvedBrushState& state,
    std::span<const reference::VectorStrokeInput> samples) {
    std::vector<BrushDab> result;
    if (state.package.profileId != "membrane-v1" || samples.empty()) return result;
    reference::StrokeOptions options;
    const auto& v = state.package.vector;
    options.size = v.size;
    options.thinning = v.thinning;
    options.smoothing = v.smoothing;
    options.streamline = v.streamline;
    options.simulate_pressure = v.pressureSource == BrushPressureSource::kSimulated;
    options.start_cap = v.startCap;
    options.end_cap = v.endCap;
    options.last = true;
    const auto points = reference::getStrokePoints(samples, options);
    if (points.empty()) return result;
    const double step = std::max(0.5, v.size * state.package.grain.spacing);
    const double total = points.back().running_length;
    const std::size_t count = total <= 0.0 ? 1U : static_cast<std::size_t>(std::floor(total / step)) + 1U;
    result.reserve(count);
    std::size_t segment = 1U;
    for (std::size_t index = 0; index < count; ++index) {
        const double distance = total <= 0.0 ? 0.0 : std::min(total, static_cast<double>(index) * step);
        while (segment < points.size() && points[segment].running_length < distance) ++segment;
        const auto& right = points[std::min(segment, points.size() - 1U)];
        const auto& left = points[segment == 0U ? 0U : segment - 1U];
        const double span = right.running_length - left.running_length;
        const double t = span > 0.0 ? (distance - left.running_length) / span : 0.0;
        const double x = left.x + (right.x - left.x) * t;
        const double y = left.y + (right.y - left.y) * t;
        const float rotation = static_cast<float>(std::atan2(right.y - left.y, right.x - left.x));
        const double size = std::max(0.01, v.size);
        const float opacity = static_cast<float>(std::clamp(
            state.package.paint.opacity * state.package.grain.opacity, 0.0, 1.0));
        result.push_back({x, y, size, rotation, opacity, 1U, 4U});
    }
    return result;
}

} // namespace canvas::ink
