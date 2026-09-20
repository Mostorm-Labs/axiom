#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "arc/arc.hpp"

#include <cassert>

namespace {
LRESULT CALLBACK Proc(HWND w, UINT m, WPARAM p, LPARAM l) {
  return DefWindowProcW(w, m, p, l);
}
[[maybe_unused]] arc_preview_target_v0 target(HWND window) {
  return {sizeof(arc_preview_target_v0), ARC_ABI_VERSION, ARC_PLATFORM_WINDOWS, 0,
          1, 1, 320, 200, 1.0F, 0, reinterpret_cast<uint64_t>(window)};
}
}  // namespace

int main() {
  WNDCLASSW klass{}; klass.hInstance = GetModuleHandleW(nullptr); klass.lpfnWndProc = Proc;
  klass.lpszClassName = L"AxiomArcPreviewBackendTest";
  assert(RegisterClassW(&klass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
  HWND window = reinterpret_cast<HWND>(1);
  auto backend = arc::CreateWindowsBackend(); assert(backend != nullptr);
  assert(backend->Attach(target(window)) == arc::Status::kOk);
  arc_preview_begin_v0 begin{}; begin.struct_size = sizeof(begin); begin.abi_version = ARC_ABI_VERSION;
  begin.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; begin.stroke_id = 9; begin.view_id = 1;
  begin.viewport_revision = 1; begin.target_generation = 1; begin.brush.struct_size = sizeof(begin.brush);
  begin.brush.abi_version = ARC_ABI_VERSION; assert(backend->Begin(begin) == arc::Status::kOk);
  arc_preview_primitive_v0 points[]{{ARC_PREVIEW_PRIMITIVE_VECTOR_POINT, 0, 10, 10, 2, 0, 1},
                                    {ARC_PREVIEW_PRIMITIVE_VECTOR_POINT, 0, 50, 50, 2, 0, 1}};
  arc_preview_update_v0 update{}; update.struct_size = sizeof(update); update.abi_version = ARC_ABI_VERSION;
  update.schema_version = ARC_PROTOCOL_SCHEMA_VERSION; update.stroke_id = 9; update.preview_revision = 1;
  update.target_generation = 1; update.confirmed_append = points; update.confirmed_append_count = 2;
  update.confirmed_append_stride = sizeof(points[0]);
  assert(backend->Push(update) == arc::Status::kOk);
  arc_preview_seal_v0 seal{sizeof(seal), ARC_ABI_VERSION, 9, 1, 1};
  assert(backend->SealInput(seal) == arc::Status::kOk);
  arc_canonical_commit_v0 commit{sizeof(commit), ARC_ABI_VERSION, 9, 1, 4, 1, {7, 8}};
  assert(backend->CanonicalCommitted(commit) == arc::Status::kOk);
  arc_canonical_visible_v0 wrong{}; wrong.struct_size = sizeof(wrong); wrong.abi_version = ARC_ABI_VERSION;
  wrong.stroke_id = 9; wrong.document_revision = 4; wrong.target_generation = 1; wrong.handoff_token = {7, 9};
  assert(backend->CanonicalVisible(wrong) == arc::Status::kInvalidState);
  wrong.handoff_token = {7, 8}; assert(backend->CanonicalVisible(wrong) == arc::Status::kOk);
  assert(backend->Detach(1) == arc::Status::kOk); return 0;
}
