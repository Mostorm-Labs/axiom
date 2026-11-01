#include "ink_playground_host.hpp"

#include <cassert>
#include <fstream>
#include <string>

namespace {

void snap_pointer_path_uses_scene_query() {
  std::ifstream source(std::string(AXIOM_INK_PLAYGROUND_SOURCE_DIR) +
                       "/common/ink_playground_host.cpp");
  assert(source.good());
  const std::string text((std::istreambuf_iterator<char>(source)),
                         std::istreambuf_iterator<char>());
  const auto begin = text.find("bool InkPlaygroundHost::selectionPointer");
  const auto end = text.find("interaction::SubmitResult InkPlaygroundHost::submit(", begin);
  assert(begin != std::string::npos && end != std::string::npos && end > begin);
  const auto hotPath = text.substr(begin, end - begin);
  assert(hotPath.find("runtimeSceneHost_->query(canvas::SceneQuery{") != std::string::npos);
  assert(hotPath.find("runtimeScene().records()") == std::string::npos);
}

}  // namespace

int main() {
  snap_pointer_path_uses_scene_query();
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
