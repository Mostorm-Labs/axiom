#!/usr/bin/env python3
"""Read-only exact-cursor source audit; this is not a product test."""
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
CURSOR = '6bcf2d4bbf3314e5252951190c3c475403f52665'
FILES = {
    'runtime/foundation/include/canvas/runtime/selection_summary.hpp': ['std::array<SelectionPropertyAggregate, 3>', 'selectionRevision', 'semanticGeneration'],
    'runtime/foundation/include/canvas/runtime/canvas_control_types.hpp': ['struct CanvasControlRequest', 'struct CanvasControlReceipt', 'kSelectedAppearance'],
    'runtime/composition/src/canvas_runtime_facade.cpp': ['singleObjectEdit&&runtime.selectedObjectCount()>1', 'semanticGeneration = runtime.semanticGeneration()'],
    'runtime/control/src/canvas_control_service.cpp': ['capturedSnapshot->selectedObject != current.selectedObject', 'capturedSnapshot->selectionRevision!=current.selectionRevision'],
    'runtime/interaction/include/canvas/interaction/canvas_editing_runtime.hpp': ['beginExplicitIntent()', 'prepareExplicitIntent(', 'commitExplicitIntent()', 'abortExplicitIntent()'],
    'debug_ui/src/controller.cpp': ['DebugControllerContext context', 'assembler_.capture(sources)'],
    'debug_ui/src/snapshot_assembler.cpp': ['sameIdentity(leading.identity, trailing.identity)', 'summary.semanticGeneration == trailing.control.semanticGeneration'],
    'debug_ui/src/control_router.cpp': ['current.semanticGeneration==frame_.semanticGeneration', 'current.sceneRevision==frame_.sceneRevision'],
    'debug_ui/include/canvas/debug_ui/controller.hpp': ['struct DebugControllerContext final', 'class DebugController final'],
    'debug_ui/CMakeLists.txt': ['add_library(canvas_debug_ui STATIC', 'target_include_directories(canvas_debug_ui PUBLIC'],
    'apps/minimal_runtime_host/windows_main.cpp': ['DebugControllerContext{state.facade.get()'],
    'apps/axiom_canvas_demo/platform/web/web_debug_ui.inl': ['controls.runtime = &composition_.controlFacade()'],
    'docs/adr/0022-runtime-public-c-api-and-cpp-style.md': ['唯一公共边界是稳定 C ABI'],
}

def main():
    entries = []
    for path, needles in FILES.items():
        data = subprocess.check_output(['git', 'show', f'{CURSOR}:{path}'], cwd=ROOT)
        content = data.decode('utf-8')
        for needle in needles:
            if needle not in content:
                raise ValueError(f'{path}: missing expected source marker {needle!r}')
        blob = subprocess.check_output(['git', 'rev-parse', f'{CURSOR}:{path}'], cwd=ROOT, text=True).strip()
        entries.append({'path': path, 'git_blob_sha': blob, 'sha256': hashlib.sha256(data).hexdigest(), 'markers': needles})
    queue = subprocess.check_output(['git', 'show', f'{CURSOR}:runtime/control/src/canvas_control_service.cpp'], cwd=ROOT, text=True)
    scope = queue[queue.index('if (capturedSnapshot.has_value())'):queue.index('if (request.payload.kind == CanvasControlPayloadKind::kSelectedAppearance)')]
    if 'semanticGeneration' in scope:
        raise ValueError('queue stale-admission source changed; revisit audit')
    output = {'subject_revision': CURSOR, 'status': 'SOURCE_MARKERS_CONFIRMED_NOT_PRODUCT_VERIFICATION', 'files': entries,
              'queue_admission_semantic_generation_checked': False}
    path = pathlib.Path(__file__).with_name('projection-source-lock.json')
    path.write_text(json.dumps(output, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'PASS source audit: {len(entries)} exact-cursor paths; semantic-generation queue gap confirmed; product tests NOT_RUN')

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(1)
