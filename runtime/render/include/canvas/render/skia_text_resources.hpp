#pragma once
#include "canvas/text/text_layout.hpp"
#include <memory>
class SkCanvas;
namespace canvas::render {
// Private Skia consumer ownership; no font handles cross Scene/Text contracts.
class SkiaTextResources final {
 public:
    SkiaTextResources();
    ~SkiaTextResources();
    void draw(SkCanvas&, const text::TextLayoutSnapshot&);
    void drawSectionTitle(SkCanvas&, const text::TextLayoutSnapshot&, float width, float height);
 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
