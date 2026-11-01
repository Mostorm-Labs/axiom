#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace canvas::runtime {
enum class ImageExportFormat : std::uint8_t { kPng, kJpeg };
enum class ImageExportBackground : std::uint8_t { kDocument, kTransparent, kSolid };
enum class ImageExportStatus : std::uint8_t {
  kExported, kInvalidArgument, kTooLarge, kWrongThread, kNotReady, kBusy,
  kStale, kResourceUnavailable, kFailed, kUnsupported
};
// Synchronous owner-thread query of committed content. No editing session is
// committed or cancelled. All returned bytes belong to the caller.
struct ImageExportOptions final {
  ImageExportFormat format = ImageExportFormat::kPng;
  ImageExportBackground background = ImageExportBackground::kDocument;
  std::array<float,4> backgroundColor{1,1,1,1};
  double scale = 1;
  double quality = .92;
  std::uint32_t padding = 16;
  bool includePattern = true;
  std::optional<std::array<double,4>> bounds;
  std::optional<std::uint64_t> expectedGeneration;
};
struct ImageExportResult final {
  ImageExportStatus status = ImageExportStatus::kUnsupported;
  std::vector<std::uint8_t> bytes;
  std::string mimeType;
  std::uint32_t width = 0, height = 0;
  std::array<double,4> bounds{};
  std::uint64_t generation = 0;
};
} // namespace canvas::runtime
