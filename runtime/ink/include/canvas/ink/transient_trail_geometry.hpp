#pragma once
#include <vector>
namespace canvas::ink {
// Derived presentation only. No timestamps or trail geometry enter a document.
struct FadingTrailPoint final {
    double x=0,y=0;
    float width=0,opacity=0;
};
struct FadingTrailPreview final {std::vector<FadingTrailPoint> points;};
}
