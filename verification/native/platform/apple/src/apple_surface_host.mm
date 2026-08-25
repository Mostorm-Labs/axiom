#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>

#include <axiom/verification/apple_harness_adapter.hpp>

namespace axiom::verification::platform {

bool apple_native_surface_seam_available() noexcept {
  // This is a host seam probe only. Product RN/Fabric owns the actual layer.
  return CAMetalLayer.class != Nil;
}

}  // namespace axiom::verification::platform
