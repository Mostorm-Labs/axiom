#include "platform_contract.hpp"

namespace canvas::ink_playground {

PlatformContract platformContract(PlatformKind platform) noexcept {
  switch (platform) {
    case PlatformKind::kWindows:
      return {platform, IngressKind::kNativeArcInputSource, false, false, false,
              "platform/windows/main.cpp"};
    case PlatformKind::kAndroid:
      return {platform, IngressKind::kNativeArcInputSource, false, false, false,
              "platform/android/bridge.cpp"};
    case PlatformKind::kWeb:
      return {platform, IngressKind::kPointerEventWasmBatch, false, false, false,
              "platform/web/bridge.cpp"};
  }
  return {platform, IngressKind::kPointerEventWasmBatch, false, false, false, {}};
}

}  // namespace canvas::ink_playground
