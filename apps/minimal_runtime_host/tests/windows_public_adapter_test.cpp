#include "canvas/platform/windows_runtime_adapter.hpp"
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "windows_d3d12_surface_provider.hpp"

#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <cassert>
#include <cstdio>
#include <thread>
#include <type_traits>

namespace {
LONG WINAPI reportNativeFailure(EXCEPTION_POINTERS* failure) {
  const auto process=GetCurrentProcess();
  SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS);
  (void)SymInitialize(process,nullptr,TRUE);
  std::fprintf(stderr,"NATIVE_FAILURE code=%08lx address=%p\n",
      static_cast<unsigned long>(failure->ExceptionRecord->ExceptionCode),
      failure->ExceptionRecord->ExceptionAddress);
  auto context=*failure->ContextRecord;
  STACKFRAME64 frame{};
  frame.AddrPC={context.Rip,0,AddrModeFlat};
  frame.AddrFrame={context.Rbp,0,AddrModeFlat};
  frame.AddrStack={context.Rsp,0,AddrModeFlat};
  for(unsigned i=0;i<32;++i){
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
    auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);
    symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=MAX_SYM_NAME;
    DWORD64 offset=0;
    if(SymFromAddr(process,frame.AddrPC.Offset,&offset,symbol))
      std::fprintf(stderr,"NATIVE_STACK %s+%llu\n",symbol->Name,static_cast<unsigned long long>(offset));
    else std::fprintf(stderr,"NATIVE_STACK address=%llu\n",static_cast<unsigned long long>(frame.AddrPC.Offset));
    if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,
        nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
  }
  std::fflush(stderr);
  return EXCEPTION_EXECUTE_HANDLER;
}
}
int main() {
  SetUnhandledExceptionFilter(reportNativeFailure);
  // External Debug UI consumers need the adapter to be a diagnostics provider.
  assert((std::is_base_of_v<canvas::runtime::PlatformDiagnostics,
                            canvas::platform::WindowsRuntimeAdapter>));
  WNDCLASSW klass{};
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = DefWindowProcW;
  klass.lpszClassName = L"AxiomPublicAdapterTest";
  assert(RegisterClassW(&klass) != 0U);
  HWND window = CreateWindowExW(0, klass.lpszClassName, L"Axiom adapter test",
      WS_OVERLAPPEDWINDOW, 0, 0, 400, 400, nullptr, nullptr, klass.hInstance, nullptr);
  assert(window);
  canvas::runtime::RuntimeComposition composition;
  assert(composition.open());
  canvas::platform::WindowsRuntimeAdapter adapter(composition, window);
  if (!adapter.bind(256, 256, 2.0F)) {
    std::fprintf(stderr, "BLOCKED_ENVIRONMENT: D3D12 provider unavailable\n");
    DestroyWindow(window);
    return 77;
  }
  const canvas::runtime::PlatformDiagnostics& diagnostics = adapter;
  const auto metrics = composition.surfaceMetrics();
  assert(metrics && metrics->available && metrics->width == 256U && metrics->height == 256U);
  assert(metrics->logicalWidth == 128.0F && metrics->logicalHeight == 128.0F);
  auto surface = diagnostics.readPlatformDiagnostics();
  assert(surface.surfaceAvailable && surface.width == 256U && surface.height == 256U);
  assert(surface.devicePixelRatio == 2.0F && surface.canonicalSurfaceGeneration != 0);
  // A provider context remains independently serviceable across replacement.
  canvas::minimal_runtime_host::WindowsD3D12SurfaceProvider completionProvider(window);
  assert(completionProvider.resize(32, 32).code == canvas::render::BackendSubmissionCode::kAccepted);
  auto completion = completionProvider.gpuCompletionService();
  assert(completion && completion->state() && !completion->state()->abandoned.load());
  const auto completionState = completion->state();
  assert(completionProvider.resize(48, 48).code == canvas::render::BackendSubmissionCode::kAccepted);
  completion->service();
  assert(completionProvider.gpuCompletionService()->state()->lifetime != completionState->lifetime);
  assert(!completionState->physicalLost.load());
  assert(adapter.prepareFrame(true));
  assert(adapter.frameStatus().available && adapter.frameStatus().canonicalWorkId != 0);
  assert(adapter.present());
  assert(!adapter.frameStatus().available);
  assert(!adapter.present());
  bool otherThreadAccepted = true;
  bool otherThreadRead = true;
  bool otherThreadObservation = true;
  std::thread other([&] { otherThreadAccepted = adapter.resize(300, 300, 1.0F);
                          otherThreadRead = composition.surfaceMetrics().has_value();
                          otherThreadObservation = composition.hostObservation().has_value(); });
  other.join();
  assert(!otherThreadAccepted && !otherThreadRead && !otherThreadObservation);
  assert(composition.registerSource(9100));
  const auto initialObservation = composition.hostObservation();
  assert(initialObservation && initialObservation->objectCount == 0U && !initialObservation->previewActive);
  assert(!composition.hoverSelectionAtViewPoint(20.0F, 20.0F));
  canvas::input::PlatformPointerBatch batch;
  canvas::input::PlatformPointerSample sample{};
  sample.source = 9100; sample.pointer = 1; sample.sequence = 1; sample.timestampNs = 1000000;
  sample.phase = canvas::input::PointerPhase::kDown; sample.x = 20; sample.y = 20;
  batch.samples = {sample};
  const auto first = composition.beginContact(batch);
  assert(first);
  sample.phase = canvas::input::PointerPhase::kMove; sample.x = 60; sample.y = 60;
  sample.sequence = 2; sample.timestampNs = 2000000; batch.samples = {sample};
  assert(composition.submitContact(*first, batch));
  assert(composition.hostObservation() && composition.hostObservation()->previewActive);
  assert(adapter.prepareFrame(false));
  assert(adapter.frameStatus().previewWorkId != 0);
  assert(adapter.present());
  sample.phase = canvas::input::PointerPhase::kUp; sample.sequence = 3;
  sample.timestampNs = 3000000; batch.samples = {sample};
  assert(composition.submitContact(*first, batch));
  assert(composition.hostObservation() && composition.hostObservation()->objectCount == 1U);
  // The startup loader must not replace a manifest after content exists.
  assert(!adapter.loadHostFonts(L"axiom-font.otf", L"fonts"));
  sample.phase = canvas::input::PointerPhase::kDown;
  sample.sequence = 4; sample.timestampNs = 4000000; batch.samples = {sample};
  const auto second = composition.beginContact(batch);
  assert(second && second->generation != first->generation);
  sample.phase = canvas::input::PointerPhase::kUp;
  sample.sequence = 100; sample.timestampNs = 5000000; batch.samples = {sample};
  assert(!composition.submitContact(*first, batch));
  sample.sequence = 5; sample.timestampNs = 6000000; batch.samples = {sample};
  assert(composition.submitContact(*second, batch));
  sample.phase = canvas::input::PointerPhase::kDown;
  sample.sequence = 6; sample.timestampNs = 7000000; batch.samples = {sample};
  const auto third = composition.beginContact(batch);
  assert(third);
  auto wrongSource = sample;
  wrongSource.source = 9999;
  wrongSource.phase = canvas::input::PointerPhase::kMove;
  batch.samples = {wrongSource};
  assert(!composition.submitContact(*third, batch));
  assert(!composition.cancelContact(*second, 7, 8000000));
  assert(composition.cancelContact(*third, 8, 9000000));
  assert(composition.unregisterSource(9100, 9, 10000000));
  assert(composition.registerSource(9100));
  sample.sequence = 10; sample.timestampNs = 11000000; batch.samples = {sample};
  const auto rebound = composition.beginContact(batch);
  assert(rebound && rebound->generation != third->generation);
  sample.phase = canvas::input::PointerPhase::kUp;
  sample.sequence = 11; sample.timestampNs = 12000000; batch.samples = {sample};
  assert(!composition.submitContact(*third, batch));
  assert(composition.submitContact(*rebound, batch));
  assert(adapter.resize(320, 240, 2.0F));
  const auto resized = composition.surfaceMetrics();
  assert(resized && resized->available && resized->logicalWidth == 160.0F &&
         resized->logicalHeight == 120.0F && resized->width == 320U && resized->height == 240U);
  surface = diagnostics.readPlatformDiagnostics();
  assert(surface.surfaceAvailable && surface.width == 320U && surface.height == 240U);
  assert(adapter.prepareFrame(true));
  assert(adapter.present());
  assert(adapter.loseSurface());
  assert(composition.surfaceMetrics() && !composition.surfaceMetrics()->available);
  assert(!diagnostics.readPlatformDiagnostics().surfaceAvailable);
  assert(!adapter.prepareFrame(true));
  assert(adapter.recoverSurface(320, 240, 2.0F));
  assert(diagnostics.readPlatformDiagnostics().surfaceAvailable);
  assert(adapter.prepareFrame(true));
  assert(adapter.present());
  assert(adapter.shutdown());
  assert(composition.close());
  assert(!adapter.prepareFrame(true));
  DestroyWindow(window);
  std::puts("PASS real HWND adapter bind, frame, resize, shutdown");
}
