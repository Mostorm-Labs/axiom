#include "ink_playground_host.hpp"

#include <cassert>

int main() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(640, 480));
  assert(host.beginBrushSession(1U));
  assert(host.appendBrushSample(1U, 20.0, 20.0, 0.5, 1U));
  assert(host.finishBrushSession(1U));
  assert(host.sceneRevision() > 0U);
  const auto semantic = host.semanticGeneration();
  const auto camera = host.cameraGeneration();
  assert(host.fitViewportToContent());
  assert(host.semanticGeneration() == semantic);
  assert(host.cameraGeneration() > camera);
  assert(host.sceneRevision() > 0U);
  return 0;
}
