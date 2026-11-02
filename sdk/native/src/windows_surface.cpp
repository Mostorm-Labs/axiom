#include "windows_surface.hpp"
#include "skia_native_platform.hpp"
#include "windows_d3d12_surface_provider.hpp"
#include <cmath>
namespace canvas::sdk {
std::unique_ptr<NativeSessionPlatform> makeWindowsNativePlatform(const NativeView& view,const std::filesystem::path& assets){
 return makeSkiaNativePlatform(view,[](const NativeView& target){
  auto canonical=std::make_unique<minimal_runtime_host::WindowsD3D12SurfaceProvider>(static_cast<HWND>(target.canonical),true);
  auto preview=std::make_unique<minimal_runtime_host::WindowsD3D12SurfaceProvider>(static_cast<HWND>(target.canonical),false);
  const auto w=static_cast<std::uint32_t>(std::round(target.width*target.dpr)),h=static_cast<std::uint32_t>(std::round(target.height*target.dpr));
  if(canonical->resize(w,h).code!=render::BackendSubmissionCode::kAccepted||preview->resize(w,h).code!=render::BackendSubmissionCode::kAccepted)return NativeSurfacePair{};
  return NativeSurfacePair{std::move(canonical),std::move(preview),"windows-d3d12-canonical","windows-d3d12-preview"};
 },assets);
}
}
