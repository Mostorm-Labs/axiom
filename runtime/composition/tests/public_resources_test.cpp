#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>
#include <thread>

int main() {
  using namespace canvas;
  runtime::RuntimeComposition owner;
  assert(owner.open() && owner.bindSurface(256, 256));
  semantic::ResourceManifest manifest;
  semantic::ResourceManifestEntry entry;
  entry.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(7)};
  entry.resource_revision = 1;
  entry.content_hash = "sha256:4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a";
  entry.byte_length = 1; entry.decode_version = 1;
  entry.metadata = {0x08, 0x01, 0x10, 0x01, 0x18, 0x01, 0x20, 0x01};
  manifest.entries.push_back(entry); manifest.manifest_revision = 1;
  auto invalid = manifest; invalid.entries.front().content_hash = "invalid";
  assert(!owner.bindInitialResourceManifest(invalid, 12));
  assert(owner.bindInitialResourceManifest(manifest, 12));
  assert(!owner.bindInitialResourceManifest(manifest, 13));
  assert(!owner.resourceToken(foundation::ObjectId::fromUint64(999)));
  const auto token = owner.resourceToken(entry.resource_id.value);
  assert(token);
  assert(!owner.publishResourceBytes(*token, {0x02}));
  assert(owner.publishResourceBytes(*token, {0x01}));
  assert(owner.ownerPump());
  const auto receipt = owner.resourceReceipt(entry.resource_id.value);
  assert(receipt && receipt->state == runtime::ResourceBindingState::kBytesValidated);
  auto stale = *token; ++stale.lifetimeGeneration;
  assert(!owner.publishResourceBytes(stale, {0x01}));
  bool observed = true;
  std::thread other([&] { observed = owner.resourceToken(entry.resource_id.value).has_value(); });
  other.join(); assert(!observed);
  assert(owner.close());
  assert(!owner.resourceToken(entry.resource_id.value));
  runtime::RuntimeComposition nonempty;
  assert(nonempty.open() && nonempty.bindSurface(256, 256) && nonempty.registerSource(1));
  input::PlatformPointerSample sample{};
  sample.source = 1; sample.pointer = 1; sample.sequence = 1; sample.timestampNs = 1;
  sample.x = 20; sample.y = 20; sample.phase = input::PointerPhase::kDown;
  input::PlatformPointerBatch batch; batch.samples.push_back(sample);
  const auto key = nonempty.beginContact(batch); assert(key);
  assert(!nonempty.bindInitialResourceManifest(manifest, 12));
  sample.sequence = 2; sample.timestampNs = 2; sample.x = 60; sample.y = 60;
  sample.phase = input::PointerPhase::kUp; batch.samples[0] = sample;
  assert(nonempty.submitContact(*key, batch));
  assert(nonempty.hostObservation()->objectCount == 1);
  assert(!nonempty.bindInitialResourceManifest(manifest, 12));
  assert(nonempty.close());
}
