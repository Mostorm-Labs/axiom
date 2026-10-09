#pragma once

#include "canvas/foundation/object_id.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace canvas::runtime {

struct EncodedResource final {
    std::uint64_t generation = 0;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
};

// Application-owned byte supply. Network, file loading and canonical operations
// are outside this renderer-neutral observation boundary.
class ResourceProvider {
  public:
    virtual ~ResourceProvider() = default;
    [[nodiscard]] virtual EncodedResource resolve(foundation::ObjectId id) const = 0;
};

class MemoryResourceProvider final : public ResourceProvider {
  public:
    void publish(foundation::ObjectId id, std::vector<std::uint8_t> bytes) {
        auto& resource = resources_[id];
        ++resource.generation;
        resource.bytes = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
    }
    void markMissing(foundation::ObjectId id) {
        auto& resource = resources_[id];
        ++resource.generation;
        resource.bytes.reset();
    }
    [[nodiscard]] EncodedResource resolve(foundation::ObjectId id) const override {
        const auto found = resources_.find(id);
        return found == resources_.end() ? EncodedResource{} : found->second;
    }
  private:
    std::map<foundation::ObjectId, EncodedResource> resources_;
};
} // namespace canvas::runtime
