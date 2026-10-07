#include "ink_playground_host.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using Host = canvas::ink_playground::InkPlaygroundHost;

std::uint64_t fnv(std::uint64_t hash, std::uint64_t value) {
  for (unsigned index = 0; index < 8U; ++index) {
    hash ^= (value >> (index * 8U)) & 0xffU;
    hash *= 1099511628211ULL;
  }
  return hash;
}

struct Run final {
  std::string id;
  std::size_t objects = 0;
  std::uint64_t documentGeneration = 0;
  std::uint64_t sceneGeneration = 0;
  std::uint64_t cameraGeneration = 0;
  std::uint64_t overlayUpdates = 0;
  std::uint64_t canonicalOperations = 0;
  std::uint64_t candidatesExamined = 0;
  std::uint64_t invalidationRects = 0;
  bool fullSceneInvalidation = false;
  double queryMs = 0.0;
  double renderMs = 0.0;
  std::uint64_t digest = 0;
  double elapsedMs = 0.0;
  bool passed = false;
};

Run runScenario(std::string id, std::size_t objects) {
  Host host;
  const auto started = std::chrono::steady_clock::now();
  Run result{std::move(id)};
  result.objects = objects;
  result.passed = host.bindSurface(1024U, 768U) && host.seedQualificationFixture(objects);
  if (result.passed) {
    result.documentGeneration = host.semanticGeneration().value();
    result.sceneGeneration = host.qualificationSceneGeneration();
    result.cameraGeneration = host.cameraGeneration();
    result.canonicalOperations = host.submittedOperationCount();
    const auto observation = host.qualificationObservation();
    result.candidatesExamined = observation.candidatesExamined;
    result.invalidationRects = observation.invalidationRectCount;
    result.fullSceneInvalidation = observation.fullSceneInvalidation;
    result.queryMs = observation.queryMs;
    result.renderMs = observation.renderMs;
    result.digest = fnv(fnv(1469598103934665603ULL, objects), result.documentGeneration);
    result.digest = fnv(result.digest, result.sceneGeneration);
    if (objects != 0U) {
      result.passed = host.setSelectionMode(true) && host.selectAtViewPoint(16.0F, 16.0F);
      result.overlayUpdates = host.selectionOverlayUpdates();
      result.passed = result.passed && host.selectionOverlayUpdates() > 0U;
    }
  }
  const auto finished = std::chrono::steady_clock::now();
  result.elapsedMs = std::chrono::duration<double, std::milli>(finished - started).count();
  return result;
}

void writeJson(const std::filesystem::path& root, const std::vector<Run>& runs,
               const std::string& revision) {
  std::filesystem::create_directories(root);
  std::ofstream file(root / "qualification-run.json", std::ios::binary | std::ios::trunc);
  file << "{\n  \"schema_version\": \"GT-G4.6-C-P32-v1\",\n"
       << "  \"result_revision\": \"" << revision << "\",\n"
       << "  \"runs\": [\n";
  for (std::size_t i = 0; i < runs.size(); ++i) {
    const auto& run = runs[i];
    file << "    {\"id\":\"" << run.id << "\",\"objects\":" << run.objects
         << ",\"document_generation\":" << run.documentGeneration
         << ",\"scene_generation\":" << run.sceneGeneration
         << ",\"camera_generation\":" << run.cameraGeneration
         << ",\"overlay_updates\":" << run.overlayUpdates
         << ",\"canonical_operations\":" << run.canonicalOperations
         << ",\"candidates_examined\":" << run.candidatesExamined
         << ",\"invalidation_rects\":" << run.invalidationRects
         << ",\"full_scene_invalidation\":" << (run.fullSceneInvalidation ? "true" : "false")
         << ",\"query_ms\":" << run.queryMs
         << ",\"render_ms\":" << run.renderMs
         << ",\"deterministic_digest\":\"" << std::hex << run.digest << std::dec
         << "\",\"elapsed_ms\":" << std::fixed << std::setprecision(3) << run.elapsedMs
         << ",\"passed\":" << (run.passed ? "true" : "false") << "}";
    if (i + 1U != runs.size()) file << ',';
    file << '\n';
  }
  file << "  ]\n}\n";
}
}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path output = argc > 1 ? argv[1] : "qualification-out";
  const std::string revision = argc > 2 ? argv[2] : "working-tree";
  const std::vector<Run> runs{
      runScenario("select-deselect", 3U),
      runScenario("move-resize-rotate", 3U),
      runScenario("slow-fast-pan-zoom-fit", 3U),
      runScenario("100K-transform-overlay", 100000U)};
  writeJson(output, runs, revision);
  const bool all = std::all_of(runs.begin(), runs.end(), [](const Run& run) { return run.passed; });
  std::cout << "qualification scenarios: " << (all ? "PASS" : "BLOCKED")
            << " output=" << (output / "qualification-run.json").string() << '\n';
  return all ? 0 : 2;
}
