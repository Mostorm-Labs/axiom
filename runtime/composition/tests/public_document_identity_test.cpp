#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>
#include <thread>

int main() {
  using namespace canvas;
  runtime::RuntimeComposition owner;
  const auto id = foundation::ObjectId::fromUint64(0x4321U);
  assert(!owner.openDocument({}));
  assert(owner.openDocument(id));
  assert(!owner.openDocument(id));
  assert(owner.documentId() == id);
  assert(owner.bindSurface(256, 256));
  semantic::ResourceManifest fonts;
  fonts.entries.push_back({semantic::ResourceId{foundation::ObjectId::fromUint64(90)},
      semantic::ResourceKind::kFont, 1,
      "sha256:4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a",
      1, 1, 1, {8, 1, 16, 0, 24, 1}});
  assert(owner.bindInitialResourceManifest(fonts, 1));
  auto& facade = owner.controlFacade();
  const auto targets = facade.mountedCanvasTargets(); assert(!targets.empty());
  runtime::TextIntent create{}; create.kind = runtime::TextIntentKind::kCreate;
  create.target = targets.front(); create.clientId = 80; create.requestId = 1;
  create.token = facade.readTextEditor().token;
  create.viewX = 30; create.viewY = 40;
  assert(facade.submitTextIntent(create).status == runtime::TextStatus::kQueued);
  assert(owner.ownerPump());
  const auto editor = facade.readTextEditor();
  assert(editor.phase == runtime::TextPhase::kEditing && editor.token.document == id);
  bool otherThreadCanRead = true;
  std::thread other([&] { otherThreadCanRead = owner.documentId().has_value(); });
  other.join(); assert(!otherThreadCanRead);
  assert(owner.close());
  assert(!owner.documentId());

  runtime::RuntimeComposition legacy;
  assert(legacy.open());
  assert(legacy.documentId() == foundation::ObjectId::fromUint64(1));
  assert(legacy.close());
}
