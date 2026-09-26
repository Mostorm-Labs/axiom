#pragma once

#include "ink_playground_host.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace canvas::ink_playground {

struct BaselineRenderPath final {
  std::string_view inputBoundary;
  std::string_view runtimeBoundary;
  std::string_view renderer;
  std::string_view surfaceType;
  std::uint64_t submissionCount = 0;
  std::uint64_t readbackCount = 0;
  std::uint64_t cpuCopyCount = 0;
  std::uint64_t presentCount = 0;
  std::string_view viewportTransformApplied = "unknown";
  std::uint64_t canonicalSurfaceGeneration = 0;
  std::uint64_t previewSurfaceGeneration = 0;
  std::uint64_t previewContentRevision = 0;
  std::uint64_t previewSubmittedRevision = 0;
  bool previewDirty = false;
  std::uint64_t previewSubmissionCount = 0;
  std::uint64_t previewPresentCount = 0;
};

inline constexpr std::string_view kPlatformBrushBaselineFixtureDigest =
    "1fe528f202cbd05ec6900f049c1c4a4e9d8fe7bcb0769a8c8b27e7b3986523ff";

[[nodiscard]] bool runPlatformBrushBaselineFixture(InkPlaygroundHost& host);

[[nodiscard]] std::string platformBrushBaselineObservationJson(
    std::string_view platform, std::string_view reality, std::string_view artifactIdentity,
    const InkPlaygroundHost& host, const BaselineRenderPath& renderPath);

}  // namespace canvas::ink_playground
