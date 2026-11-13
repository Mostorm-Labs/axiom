#pragma once
#include "canvas/scene/scene_compiler.hpp"
#include "canvas/semantic/operation.hpp"
#include <memory>
namespace canvas::interaction {
// Intent-owned cursor. The immutable Operation alias keeps its exact prepared
// owner alive; mask pointers borrow only that owner or the retained before view.
struct PendingSceneStampPreparation final {
 struct Job final {
  RuntimeSceneRecord before;
  std::shared_ptr<const semantic::Operation> operation;
  std::string body;
  std::vector<const semantic::EraseMaskRecord*> masks;
  std::vector<std::uint32_t> hashes;
  std::uint32_t hash=2166136261U;
  std::size_t mask=0,primitive=0;
  bool header=false,ready=false;
 };
 std::shared_ptr<const semantic::Operation> operation;
 std::vector<std::shared_ptr<Job>> jobs;
 std::size_t target=0;
 bool failed=false;
};
bool preparePendingMaskStamps(canvas::ISemanticSceneCompiler&,
    const std::vector<RuntimeSceneRecord>&,std::shared_ptr<const semantic::Operation>,
    PendingSceneStampPreparation&,std::size_t maxSteps,std::uint64_t maxMicroseconds);
}
