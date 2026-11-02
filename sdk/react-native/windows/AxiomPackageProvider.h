#pragma once
#include "AxiomModule.h"
#include "AxiomCanvasView.h"
#include <winrt/Microsoft.ReactNative.h>
#include <winrt/Windows.Foundation.h>
namespace MostormAxiom {
struct AxiomPackageProvider{
 void CreatePackage(const winrt::Microsoft::ReactNative::IReactPackageBuilder& builder)noexcept{
  AddAttributedModules(builder, true);
  RegisterAxiomCanvasViewNativeComponent<AxiomCanvasView>(builder, nullptr);
 }
};
}
