#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace canvas::ink {

struct ResourceId final {
  std::uint64_t value = 0;
  [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }
  bool operator==(const ResourceId&) const = default;
};

enum class BrushFamily : std::uint8_t {
  kPen = 1,
  kPencil = 2,
  kChalk = 3,
  kMarker = 4,
  kWaterColorLite = 5,
  kHighlighter = 6,
  kLaser = 7,
};

enum class BrushRepresentation : std::uint8_t {
  kVector = 1,
  kDab = 2,
  kTemporalTransient = 3,
};

struct BrushCapabilityProfile final {
  bool pressure = false;
  bool tilt = false;
  bool shapeResource = false;
  bool grainResource = false;
  bool temporalTransient = false;
};

struct BrushDefinition final {
  std::uint64_t definitionId = 0;
  std::uint32_t version = 1;
  BrushFamily family = BrushFamily::kPen;
  float nominalSize = 1.0F;
  float opacity = 1.0F;
  float spacing = 0.1F;
  float pressureSizeInfluence = 0.0F;
  float pressureOpacityInfluence = 0.0F;
  float tiltSizeInfluence = 0.0F;
  float tiltRotationInfluence = 0.0F;
  ResourceId shapeResource{};
  ResourceId grainResource{};
};

class BrushProgram final {
 public:
  [[nodiscard]] std::uint64_t identity() const noexcept { return identity_; }
  [[nodiscard]] BrushRepresentation representation() const noexcept {
    return representation_;
  }
  [[nodiscard]] const BrushDefinition& definition() const noexcept {
    return definition_;
  }

 private:
  friend class BrushCompiler;
  BrushDefinition definition_{};
  BrushRepresentation representation_ = BrushRepresentation::kVector;
  std::uint64_t identity_ = 0;
};

enum class BrushCompileError : std::uint8_t {
  kNone = 0,
  kInvalidDefinition,
  kUnsupportedVersion,
  kUnsupportedCapability,
};

struct BrushCompileResult final {
  std::shared_ptr<const BrushProgram> program;
  BrushCompileError error = BrushCompileError::kNone;
  [[nodiscard]] explicit operator bool() const noexcept {
    return program != nullptr && error == BrushCompileError::kNone;
  }
};

class BrushCompiler final {
 public:
  [[nodiscard]] BrushCompileResult compile(
      const BrushDefinition& definition,
      const BrushCapabilityProfile& capabilities) const;
};

enum class BrushResourceKind : std::uint8_t { kShape = 1, kGrain = 2 };

class ResourceCatalog final {
 public:
  bool add(ResourceId id, BrushResourceKind kind);
  [[nodiscard]] bool contains(ResourceId id) const noexcept;
  [[nodiscard]] bool contains(ResourceId id, BrushResourceKind kind) const noexcept;

 private:
  std::unordered_map<std::uint64_t, BrushResourceKind> resources_;
};

struct BrushSessionId final {
  std::uint64_t value = 0;
  bool operator==(const BrushSessionId&) const = default;
};

struct BrushInputSample final {
  float x = 0.0F;
  float y = 0.0F;
  float pressure = 1.0F;
  float tiltX = 0.0F;
  float tiltY = 0.0F;
  std::uint64_t sequence = 0;
};

enum class RandomChannel : std::uint8_t {
  kSize = 1,
  kOpacity = 2,
  kRotation = 3,
  kScatterX = 4,
  kScatterY = 5,
};

[[nodiscard]] float deterministicChannel(std::uint64_t seed,
                                         RandomChannel channel,
                                         std::uint64_t sampleIndex) noexcept;

struct BrushPrimitive final {
  float x = 0.0F;
  float y = 0.0F;
  float size = 0.0F;
  float rotation = 0.0F;
  float opacity = 0.0F;
  BrushRepresentation representation = BrushRepresentation::kVector;
  ResourceId shapeResource{};
  ResourceId grainResource{};
  bool operator==(const BrushPrimitive&) const = default;
};

struct BrushCommitArtifact final {
  BrushRepresentation representation = BrushRepresentation::kVector;
  std::vector<BrushPrimitive> primitives;
  std::vector<BrushInputSample> sourceSamples;
  std::uint64_t deterministicSeed = 0;
  std::uint64_t digest = 0;
  bool canonicalMutation = false;
};

struct BrushPreviewArtifact final {
  BrushSessionId session{};
  std::vector<BrushPrimitive> primitives;
  std::uint64_t revision = 0;
  std::uint64_t durationMs = 0;
};

enum class BrushRuntimeError : std::uint8_t {
  kNone = 0,
  kInvalidSession,
  kDuplicateSession,
  kInvalidSample,
  kMissingResource,
  kResourceKindMismatch,
};

struct BrushRuntimeResult final {
  BrushCommitArtifact commit;
  BrushPreviewArtifact preview;
  BrushRuntimeError error = BrushRuntimeError::kNone;
  [[nodiscard]] explicit operator bool() const noexcept {
    return error == BrushRuntimeError::kNone;
  }
};

class BrushRuntime final {
 public:
  explicit BrushRuntime(const ResourceCatalog& resources) noexcept
      : resources_(resources) {}
  bool begin(BrushSessionId session, const BrushProgram& program,
             std::uint64_t deterministicSeed);
  [[nodiscard]] BrushRuntimeResult append(
      BrushSessionId session, std::span<const BrushInputSample> samples);
  [[nodiscard]] BrushRuntimeResult finish(BrushSessionId session);
  bool cancel(BrushSessionId session) noexcept;
  [[nodiscard]] BrushRuntimeError lastError() const noexcept { return lastError_; }
  [[nodiscard]] std::size_t activeSessionCount() const noexcept {
    return sessions_.size();
  }

 private:
  struct Session final {
    const BrushProgram* program = nullptr;
    std::uint64_t seed = 0;
    std::uint64_t revision = 0;
    std::uint64_t lastSequence = 0;
    std::vector<BrushInputSample> samples;
  };
  [[nodiscard]] BrushRuntimeResult evaluate(BrushSessionId session,
                                            const Session& state) const;
  const ResourceCatalog& resources_;
  std::unordered_map<std::uint64_t, Session> sessions_;
  BrushRuntimeError lastError_ = BrushRuntimeError::kNone;
};

}  // namespace canvas::ink
