#pragma once
#include "native_session.hpp"
#include <canvas/render/skia_surface_provider.hpp>
#include <filesystem>
namespace canvas::sdk {
struct NativeSurfacePair final {
 std::unique_ptr<render::SkiaSurfaceProvider> canonical,preview;
 std::string canonicalProfile,previewProfile;
};
using NativeSurfaceFactory=std::function<NativeSurfacePair(const NativeView&)>;
std::unique_ptr<NativeSessionPlatform> makeSkiaNativePlatform(const NativeView&,NativeSurfaceFactory,const std::filesystem::path& assetRoot);
}
