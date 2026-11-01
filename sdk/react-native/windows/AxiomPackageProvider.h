#pragma once
#include "AxiomModule.h"
#include "AxiomCanvasView.h"
#include <winrt/Microsoft.ReactNative.h>
#include <winrt/Windows.Foundation.h>
namespace MostormAxiom {
struct AxiomPackageProvider:winrt::implements<AxiomPackageProvider,winrt::Microsoft::ReactNative::IReactPackageProvider>{
 void CreatePackage(const winrt::Microsoft::ReactNative::IReactPackageBuilder& builder)noexcept{
  Microsoft::ReactNative::AddAttributedModules(builder);
  RegisterAxiomCanvasViewNativeComponent<AxiomCanvasView>(builder);
 }
};
}
