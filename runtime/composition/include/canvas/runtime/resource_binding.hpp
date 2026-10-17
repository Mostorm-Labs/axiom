#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/runtime/resource_provider.hpp"
#include "canvas/semantic/resource_manifest.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace canvas::runtime {

enum class ResourceBindingState : std::uint8_t {
  kMissing,
  kBytesValidated,
  kDecodeReady,
  kLayoutReady,
  kFailed,
  kEvicted,
  kRetryable,
  kProviderReplaced,
};

struct ResourceBindingToken final {
  foundation::ObjectId resourceId{};
  std::uint64_t documentBinding = 0;
  std::uint64_t providerEpoch = 0;
  semantic::ResourceRevision resourceVersion = 0;
  std::string expectedSha256;
  std::uint64_t requestId = 0;
  std::uint64_t lifetimeGeneration = 0;
  bool operator==(const ResourceBindingToken&) const = default;
};

struct ResourceBindingSnapshot final {
  semantic::ResourceManifest manifest{};
  std::uint64_t documentBinding = 0;
  std::uint64_t lifetimeGeneration = 1;

  [[nodiscard]] const semantic::ResourceManifestEntry* find(
      foundation::ObjectId id) const noexcept;
  [[nodiscard]] std::optional<ResourceBindingToken> token(
      foundation::ObjectId id, std::uint64_t providerEpoch,
      std::uint64_t requestId) const;
  [[nodiscard]] bool bind(const semantic::ResourceManifest& source,
                          std::uint64_t binding) noexcept;
  void invalidate() noexcept { ++lifetimeGeneration; }
};

struct ResourcePortReceipt final {
  foundation::ObjectId resourceId{};
  ResourceBindingToken token{};
  ResourceBindingState state = ResourceBindingState::kMissing;
  std::uint64_t providerGeneration = 0;
  std::size_t byteLength = 0;
  bool changed = false;
};

enum class ResourcePortError : std::uint8_t {
  kNone,
  kWrongThread,
  kNotReady,
  kUnknownResource,
  kQueueFull,
  kStaleBinding,
  kValidationFailed,
};

class ResourcePort {
 public:
  virtual ~ResourcePort() = default;
  [[nodiscard]] virtual EncodedResource pull(foundation::ObjectId id) const = 0;
};

class ResourcePortAdapter final : public ResourcePort {
 public:
  explicit ResourcePortAdapter(const ResourceProvider& provider) noexcept
      : provider_(provider) {}
  [[nodiscard]] EncodedResource pull(foundation::ObjectId id) const override {
    return provider_.resolve(id);
  }
 private:
  const ResourceProvider& provider_;
};

[[nodiscard]] bool validateResourceBytes(
    const semantic::ResourceManifestEntry& entry,
    const EncodedResource& resource) noexcept;

}  // namespace canvas::runtime
