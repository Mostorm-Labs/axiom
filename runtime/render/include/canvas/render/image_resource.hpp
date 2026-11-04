#pragma once

#include "canvas/runtime/resource_provider.hpp"
#include "canvas/semantic/object_content.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

template <typename T> class sk_sp;
class SkImage;
namespace canvas::render {
enum class ImageResourceState { kMissing, kReady, kDecodeFailed };
struct DecodedImage final {
    semantic::ResourceId resourceId{};
    std::uint64_t generation = 0;
    ImageResourceState state = ImageResourceState::kMissing;
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
    std::string diagnostic;
    std::string mimeType;
};
class ImageResourceResolver final {
  public:
    explicit ImageResourceResolver(const runtime::ResourceProvider& provider);
    ~ImageResourceResolver();
    [[nodiscard]] std::shared_ptr<const DecodedImage> resolve(semantic::ResourceId id);
    [[nodiscard]] sk_sp<SkImage> resolveSkImage(semantic::ResourceId id);
    [[nodiscard]] std::uint64_t decodeCount() const;
    [[nodiscard]] std::uint64_t imageBuildCount() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace canvas::render
