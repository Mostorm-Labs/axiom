#include "canvas/ink/brush_commit_intent.hpp"
#include "canvas/ink/brush_preview_delta.hpp"

#include <cassert>

int main() {
    canvas::ink::BrushPreviewDelta preview;
    preview.revision = 4;
    canvas::ink::BrushCommitIntent intent;
    intent.session = 9;
    intent.revision = preview.revision;
    intent.seed = 42;
    intent.confirmed.push_back({1.0, 2.0, 0.5, true, 1});
    assert(intent.confirmed.size() == 1U);
    // Prediction is not a member of the intent boundary by construction.
    assert(intent.seed == 42U && intent.revision == preview.revision);
    return 0;
}
