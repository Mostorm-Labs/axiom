#include <axiom/verification/platform_host_common.hpp>

namespace axiom::verification::platform {

VerificationBuildManifest verification_build_manifest() noexcept {
  return {.verification_only = true, .protocol_version = 1,
          .product_public_abi = false};
}

}  // namespace axiom::verification::platform
