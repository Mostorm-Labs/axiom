#include "canvas/ink/brush_preview_handoff.hpp"
#include <cassert>
int main() {
  canvas::ink::BrushPreviewHandoff handoff(2);
  canvas::ink::BrushPreviewDelta first; first.revision = 1;
  canvas::ink::BrushPreviewDelta second; second.revision = 2;
  assert(handoff.publish(1, first));
  assert(handoff.publish(1, second));
  canvas::ink::BrushCommitIntent intent; intent.session = 1; intent.revision = 2;
  assert(handoff.seal(1, intent));
  assert(handoff.pendingTerminalCount() == 1);
  assert(handoff.acknowledge({1, 1, 1}) == false);
  assert(handoff.acknowledge({1, 2, 1}));
  assert(handoff.pendingTerminalCount() == 0);
  return 0;
}
