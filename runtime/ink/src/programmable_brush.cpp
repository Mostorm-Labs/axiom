#include "canvas/ink/programmable_brush.hpp"

#include <bit>
#include <cmath>
#include <limits>

namespace canvas::ink {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void hashWord(std::uint64_t& hash, std::uint64_t word) noexcept {
  for (unsigned shift = 0; shift < 64; shift += 8) {
    hash ^= (word >> shift) & 0xffU;
    hash *= kFnvPrime;
  }
}

void hashFloat(std::uint64_t& hash, float value) noexcept {
  hashWord(hash, std::bit_cast<std::uint32_t>(value));
}

bool finiteUnit(float value) noexcept {
  return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

bool isDab(BrushFamily family) noexcept {
  return family == BrushFamily::kPencil || family == BrushFamily::kChalk ||
         family == BrushFamily::kMarker || family == BrushFamily::kWaterColorLite;
}

std::uint64_t programIdentity(const BrushDefinition& value,
                              BrushRepresentation representation) noexcept {
  std::uint64_t hash = kFnvOffset;
  hashWord(hash, value.definitionId);
  hashWord(hash, value.version);
  hashWord(hash, static_cast<std::uint8_t>(value.family));
  hashWord(hash, static_cast<std::uint8_t>(representation));
  hashFloat(hash, value.nominalSize);
  hashFloat(hash, value.opacity);
  hashFloat(hash, value.spacing);
  hashFloat(hash, value.pressureSizeInfluence);
  hashFloat(hash, value.pressureOpacityInfluence);
  hashFloat(hash, value.tiltSizeInfluence);
  hashFloat(hash, value.tiltRotationInfluence);
  hashWord(hash, value.shapeResource.value);
  hashWord(hash, value.grainResource.value);
  return hash == 0 ? 1 : hash;
}

std::uint64_t primitiveDigest(std::span<const BrushPrimitive> primitives,
                              std::uint64_t seed,
                              BrushRepresentation representation) noexcept {
  std::uint64_t hash = kFnvOffset;
  hashWord(hash, seed);
  hashWord(hash, static_cast<std::uint8_t>(representation));
  for (const auto& primitive : primitives) {
    hashFloat(hash, primitive.x);
    hashFloat(hash, primitive.y);
    hashFloat(hash, primitive.size);
    hashFloat(hash, primitive.rotation);
    hashFloat(hash, primitive.opacity);
    hashWord(hash, primitive.shapeResource.value);
    hashWord(hash, primitive.grainResource.value);
  }
  return hash;
}

}  // namespace

BrushCompileResult BrushCompiler::compile(
    const BrushDefinition& definition,
    const BrushCapabilityProfile& capabilities) const {
  if (definition.version != 1U) {
    return {nullptr, BrushCompileError::kUnsupportedVersion};
  }
  const auto familyValue = static_cast<std::uint8_t>(definition.family);
  if (definition.definitionId == 0 || familyValue < 1U || familyValue > 7U ||
      !std::isfinite(definition.nominalSize) || definition.nominalSize <= 0.0F ||
      !finiteUnit(definition.opacity) || !std::isfinite(definition.spacing) ||
      definition.spacing <= 0.0F || definition.spacing > 4.0F ||
      !finiteUnit(definition.pressureSizeInfluence) ||
      !finiteUnit(definition.pressureOpacityInfluence) ||
      !finiteUnit(definition.tiltSizeInfluence) ||
      !finiteUnit(definition.tiltRotationInfluence)) {
    return {nullptr, BrushCompileError::kInvalidDefinition};
  }
  if ((definition.pressureSizeInfluence > 0.0F ||
       definition.pressureOpacityInfluence > 0.0F) &&
      !capabilities.pressure) {
    return {nullptr, BrushCompileError::kUnsupportedCapability};
  }
  if ((definition.tiltSizeInfluence > 0.0F ||
       definition.tiltRotationInfluence > 0.0F) &&
      !capabilities.tilt) {
    return {nullptr, BrushCompileError::kUnsupportedCapability};
  }

  BrushRepresentation representation = BrushRepresentation::kVector;
  if (isDab(definition.family)) {
    representation = BrushRepresentation::kDab;
    if (!definition.shapeResource.valid() || !definition.grainResource.valid() ||
        !capabilities.shapeResource || !capabilities.grainResource) {
      return {nullptr, BrushCompileError::kUnsupportedCapability};
    }
  } else if (definition.family == BrushFamily::kLaser) {
    representation = BrushRepresentation::kTemporalTransient;
    if (!capabilities.temporalTransient) {
      return {nullptr, BrushCompileError::kUnsupportedCapability};
    }
  } else if (definition.shapeResource.valid() || definition.grainResource.valid()) {
    return {nullptr, BrushCompileError::kInvalidDefinition};
  }

  auto program = std::make_shared<BrushProgram>();
  program->definition_ = definition;
  program->representation_ = representation;
  program->identity_ = programIdentity(definition, representation);
  return {std::move(program), BrushCompileError::kNone};
}

bool ResourceCatalog::add(ResourceId id, BrushResourceKind kind) {
  return id.valid() && resources_.emplace(id.value, kind).second;
}

bool ResourceCatalog::contains(ResourceId id) const noexcept {
  return id.valid() && resources_.contains(id.value);
}

bool ResourceCatalog::contains(ResourceId id, BrushResourceKind kind) const noexcept {
  const auto found = resources_.find(id.value);
  return id.valid() && found != resources_.end() && found->second == kind;
}

float deterministicChannel(std::uint64_t seed, RandomChannel channel,
                           std::uint64_t sampleIndex) noexcept {
  std::uint64_t value = seed;
  value ^= 0x9e3779b97f4a7c15ULL *
           (static_cast<std::uint64_t>(channel) + 0x100ULL);
  value ^= 0xbf58476d1ce4e5b9ULL * (sampleIndex + 1ULL);
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return static_cast<float>(value >> 40U) / 16777216.0F;
}

bool BrushRuntime::begin(BrushSessionId session, const BrushProgram& program,
                         std::uint64_t deterministicSeed) {
  lastError_ = BrushRuntimeError::kNone;
  if (session.value == 0) {
    lastError_ = BrushRuntimeError::kInvalidSession;
    return false;
  }
  if (sessions_.contains(session.value)) {
    lastError_ = BrushRuntimeError::kDuplicateSession;
    return false;
  }
  const auto& definition = program.definition();
  if (definition.shapeResource.valid() && !resources_.contains(definition.shapeResource)) {
    lastError_ = BrushRuntimeError::kMissingResource;
    return false;
  }
  if (definition.grainResource.valid() && !resources_.contains(definition.grainResource)) {
    lastError_ = BrushRuntimeError::kMissingResource;
    return false;
  }
  if (definition.shapeResource.valid() &&
      !resources_.contains(definition.shapeResource, BrushResourceKind::kShape)) {
    lastError_ = BrushRuntimeError::kResourceKindMismatch;
    return false;
  }
  if (definition.grainResource.valid() &&
      !resources_.contains(definition.grainResource, BrushResourceKind::kGrain)) {
    lastError_ = BrushRuntimeError::kResourceKindMismatch;
    return false;
  }
  sessions_.emplace(session.value, Session{&program, deterministicSeed, 0, 0, {}});
  return true;
}

BrushRuntimeResult BrushRuntime::append(
    BrushSessionId session, std::span<const BrushInputSample> samples) {
  const auto found = sessions_.find(session.value);
  if (found == sessions_.end() || samples.empty()) {
    lastError_ = BrushRuntimeError::kInvalidSession;
    return {{}, {}, lastError_};
  }
  std::uint64_t previousSequence = found->second.lastSequence;
  for (const auto& sample : samples) {
    if (sample.sequence == 0 || sample.sequence <= previousSequence ||
        !std::isfinite(sample.x) || !std::isfinite(sample.y) ||
        !finiteUnit(sample.pressure) || !std::isfinite(sample.tiltX) ||
        !std::isfinite(sample.tiltY)) {
      lastError_ = BrushRuntimeError::kInvalidSample;
      return {{}, {}, lastError_};
    }
    previousSequence = sample.sequence;
  }
  for (const auto& sample : samples) {
    found->second.samples.push_back(sample);
    found->second.lastSequence = sample.sequence;
  }
  ++found->second.revision;
  lastError_ = BrushRuntimeError::kNone;
  return evaluate(session, found->second);
}

BrushRuntimeResult BrushRuntime::finish(BrushSessionId session) {
  const auto found = sessions_.find(session.value);
  if (found == sessions_.end() || found->second.samples.empty()) {
    lastError_ = BrushRuntimeError::kInvalidSession;
    return {{}, {}, lastError_};
  }
  auto result = evaluate(session, found->second);
  sessions_.erase(found);
  lastError_ = BrushRuntimeError::kNone;
  return result;
}

bool BrushRuntime::cancel(BrushSessionId session) noexcept {
  return sessions_.erase(session.value) == 1U;
}

BrushRuntimeResult BrushRuntime::evaluate(BrushSessionId session,
                                          const Session& state) const {
  BrushRuntimeResult result;
  const auto& program = *state.program;
  const auto& definition = program.definition();
  result.commit.representation = program.representation();
  result.commit.deterministicSeed = state.seed;
  result.commit.sourceSamples = state.samples;
  result.commit.canonicalMutation =
      program.representation() != BrushRepresentation::kTemporalTransient;
  result.preview.session = session;
  result.preview.revision = state.revision;
  if (program.representation() == BrushRepresentation::kTemporalTransient) {
    result.preview.durationMs = 650;
  }
  for (std::size_t index = 0; index < state.samples.size(); ++index) {
    const auto& sample = state.samples[index];
    const float pressureSize =
        1.0F + definition.pressureSizeInfluence * (sample.pressure - 1.0F);
    const float pressureOpacity =
        1.0F + definition.pressureOpacityInfluence * (sample.pressure - 1.0F);
    BrushPrimitive primitive;
    primitive.x = sample.x;
    primitive.y = sample.y;
    primitive.size = definition.nominalSize * pressureSize *
                     (0.85F + 0.3F * deterministicChannel(
                                             state.seed, RandomChannel::kSize, index));
    primitive.rotation =
        6.283185307F * deterministicChannel(state.seed, RandomChannel::kRotation, index) +
        sample.tiltX * definition.tiltRotationInfluence;
    primitive.opacity = definition.opacity * pressureOpacity *
                        (0.9F + 0.1F * deterministicChannel(
                                            state.seed, RandomChannel::kOpacity, index));
    primitive.representation = program.representation();
    primitive.shapeResource = definition.shapeResource;
    primitive.grainResource = definition.grainResource;
    result.preview.primitives.push_back(primitive);
    if (result.commit.canonicalMutation) result.commit.primitives.push_back(primitive);
  }
  result.commit.digest = primitiveDigest(result.commit.primitives, state.seed,
                                         program.representation());
  return result;
}

}  // namespace canvas::ink
