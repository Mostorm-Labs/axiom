#pragma once

#include "canvas/runtime/resource_provider.hpp"
#include "canvas/semantic/object_content.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace canvas::render {
enum class ImageResourceState { kMissing, kReady, kDecodeFailed };
struct DecodedImage final {
    semantic::ResourceId resourceId{};
    std::uint64_t generation = 0;
    ImageResourceState state = ImageResourceState::kMissing;
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
    std::string diagnostic;
};
class ImageResourceResolver final {
  public:
    explicit ImageResourceResolver(const runtime::ResourceProvider& provider) : provider_(provider) {}
    [[nodiscard]] std::shared_ptr<const DecodedImage> resolve(semantic::ResourceId id);
    [[nodiscard]] std::uint64_t decodeCount() const { return decodes_; }
  private:
    const runtime::ResourceProvider& provider_;
    std::map<foundation::ObjectId, std::shared_ptr<const DecodedImage>> decoded_;
    std::uint64_t decodes_ = 0;
};
} // namespace canvas::render
