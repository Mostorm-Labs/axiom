#include "canvas/runtime/resource_binding.hpp"

#include "sha256_internal.hpp"

#include <algorithm>

namespace canvas::runtime {

const semantic::ResourceManifestEntry* ResourceBindingSnapshot::find(
    foundation::ObjectId id) const noexcept {
  const auto it = std::find_if(manifest.entries.begin(), manifest.entries.end(),
      [id](const auto& entry) { return entry.resource_id.value == id; });
  return it == manifest.entries.end() ? nullptr : &*it;
}

std::optional<ResourceBindingToken> ResourceBindingSnapshot::token(
    foundation::ObjectId id, std::uint64_t providerEpoch,
    std::uint64_t requestId) const {
  const auto* entry = find(id);
  if (entry == nullptr) return std::nullopt;
  return ResourceBindingToken{documentBinding, providerEpoch,
                              entry->resource_revision, entry->content_hash,
                              requestId, lifetimeGeneration};
}

bool ResourceBindingSnapshot::bind(const semantic::ResourceManifest& source,
                                   std::uint64_t binding) noexcept {
  if (!semantic::validateResourceManifest(source) || binding == 0U) return false;
  manifest = source;
  documentBinding = binding;
  lifetimeGeneration = 1U;
  return true;
}

bool validateResourceBytes(const semantic::ResourceManifestEntry& entry,
                          const EncodedResource& resource) noexcept {
  if (!resource.bytes || resource.bytes->size() != entry.byte_length) return false;
  if (entry.content_hash.size() != 71U || entry.content_hash.rfind("sha256:", 0) != 0U) return false;
  const auto digest = semantic::internal::sha256(*resource.bytes);
  static constexpr char hex[] = "0123456789abcdef";
  std::string actual = "sha256:";
  for (const auto byte : digest) {
    actual.push_back(hex[(byte >> 4U) & 0x0FU]);
    actual.push_back(hex[byte & 0x0FU]);
  }
  return actual == entry.content_hash;
}

}  // namespace canvas::runtime
