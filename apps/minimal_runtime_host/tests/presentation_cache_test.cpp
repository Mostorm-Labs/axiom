#include "../windows_presentation_cache.hpp"

#include <cassert>

int main() {
  canvas::minimal_runtime_host::WindowsPresentationCache cache;
  assert(cache.shouldReadCanonical(1U, 7U, 16U));
  cache.markCanonicalRead(1U, 7U, std::vector<std::uint8_t>(16U, 0x11U));
  assert(!cache.shouldReadCanonical(1U, 7U, 16U));
  assert(cache.shouldReadCanonical(2U, 7U, 16U));

  assert(cache.shouldReadPreview(3U, 4U, 16U));
  cache.markPreviewRead(3U, 4U, std::vector<std::uint8_t>(16U, 0x22U));
  assert(!cache.shouldReadPreview(3U, 4U, 16U));
  assert(cache.shouldReadPreview(4U, 5U, 16U));

  cache.markComposite(1U, 7U, 3U, 4U, true,
                      std::vector<std::uint8_t>(16U, 0x33U));
  assert(!cache.shouldRecompose(1U, 7U, 3U, 4U, true));
  assert(cache.shouldRecompose(1U, 7U, 3U, 5U, true));
  cache.reset();
  assert(cache.shouldReadCanonical(1U, 7U, 16U));
  return 0;
}
