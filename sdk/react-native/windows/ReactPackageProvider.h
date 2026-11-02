#pragma once
#include "ReactPackageProvider.g.h"
namespace winrt::MostormAxiom::implementation {
struct ReactPackageProvider:ReactPackageProviderT<ReactPackageProvider> {
 void CreatePackage(winrt::Microsoft::ReactNative::IReactPackageBuilder const&) noexcept;
};
}
namespace winrt::MostormAxiom::factory_implementation {
struct ReactPackageProvider:ReactPackageProviderT<ReactPackageProvider,implementation::ReactPackageProvider> {};
}
