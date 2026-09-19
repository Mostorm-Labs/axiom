#pragma once

#include <cstdint>
#include <string_view>

namespace canvas::ink_playground {

enum class PlatformKind : std::uint8_t { kWindows, kWeb, kAndroid };
enum class IngressKind : std::uint8_t { kNativeArcInputSource, kPointerEventWasmBatch };

struct PlatformContract final {
  PlatformKind platform;
  IngressKind ingress;
  bool platformOwnsBrushSemantics = false;
  bool platformOwnsSelectionSemantics = false;
  bool platformOwnsEraserSemantics = false;
  std::string_view entrypoint;
};

[[nodiscard]] PlatformContract platformContract(PlatformKind platform) noexcept;

}  // namespace canvas::ink_playground
