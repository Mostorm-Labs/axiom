#include <windows.h>
#include <unknwn.h>
#include <winrt/base.h>
#include <CppWinRTIncludes.h>
#include <winrt/Microsoft.ReactNative.h>
#include "ReactPackageProvider.h"
#if __has_include("ReactPackageProvider.g.cpp")
#include "ReactPackageProvider.g.cpp"
#endif
#include "AxiomPackageProvider.h"
void winrt::MostormAxiom::implementation::ReactPackageProvider::CreatePackage(
 const winrt::Microsoft::ReactNative::IReactPackageBuilder& builder) noexcept {
 ::MostormAxiom::AxiomPackageProvider{}.CreatePackage(builder);
}
