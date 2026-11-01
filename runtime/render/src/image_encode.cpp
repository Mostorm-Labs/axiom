#include "canvas/render/image_encode.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"
#include "include/encode/SkJpegEncoder.h"

namespace canvas::render {
namespace {
class BoundedImageStream final : public SkWStream {
 public:
  std::vector<std::uint8_t> bytes;
  bool write(const void* data, size_t size) override {
    constexpr std::size_t limit = 128U * 1024U * 1024U;
    if (size > limit - bytes.size()) return false;
    if (size) {
      const auto* first = static_cast<const std::uint8_t*>(data);
      bytes.insert(bytes.end(), first, first + size);
    }
    return true;
  }
  size_t bytesWritten() const override { return bytes.size(); }
};
}
std::vector<std::uint8_t> encodeRasterImage(
    RasterSkiaSurfaceProvider& provider, bool jpeg, int quality) {
  const auto acquired = provider.acquire();
  if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || !acquired.frame.surface) return {};
  struct Release { RasterSkiaSurfaceProvider& provider; ~Release() { provider.release(); } } release{provider};
  SkPixmap pixels;
  if (!acquired.frame.surface->peekPixels(&pixels)) return {};
  BoundedImageStream stream;
  bool ok;
  if (jpeg) {
    SkJpegEncoder::Options options; options.fQuality = quality;
    ok = SkJpegEncoder::Encode(&stream, pixels, options);
  } else {
    ok = SkPngEncoder::Encode(&stream, pixels, {});
  }
  return ok ? std::move(stream.bytes) : std::vector<std::uint8_t>{};
}
}
