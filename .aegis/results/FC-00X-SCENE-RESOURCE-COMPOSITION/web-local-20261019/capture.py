"""Capture local-only Web preparation evidence without changing old results."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from urllib.request import urlopen

root = Path.cwd()
out = root / 'out/p32-web'
destination = root / '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/web-local-20261019'
destination.mkdir(parents=True, exist_ok=True)
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
assert subprocess.run(['git', 'diff', '--quiet']).returncode == 0
skia_records = [json.loads(line) for line in (root / 'out/p32-web-skia-fetch.log').read_text().splitlines()
                if line.startswith('{')]
if (out / 'skia-fetch.json').read_text().startswith('###'):
    shutil.copy2(out / 'skia-fetch.json', out / 'skia-fetch-summary.md')
(out / 'skia-fetch.json').write_text(json.dumps(skia_records[-1], indent=2) + '\n')
artifacts = []
for directory, names in {
    'ink_playground': ['index.html', 'axiom_ink_playground_web.js', 'axiom_ink_playground_web.wasm', 'evidence_worker.js'],
    'axiom_canvas_demo': ['index.html', 'axiom_canvas_demo_web.js', 'axiom_canvas_demo_web.wasm'],
}.items():
    for name in names:
        path = out / 'build/apps' / directory / name
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        url = f'http://127.0.0.1:8765/{directory}/{name}'
        with urlopen(url, timeout=10) as response:
            fetched = response.read()
            assert response.status == 200 and fetched == data
            assert response.headers.get('Cache-Control') == 'no-store'
            if name.endswith('.wasm'):
                assert response.headers.get_content_type() == 'application/wasm'
                assert data[:4] == b'\x00asm'
            content_type = response.headers.get_content_type()
        artifacts.append({'path': str(path), 'bytes': len(data), 'sha256': digest,
                          'url': url, 'http_status': 200, 'content_type': content_type})
result = {
    'source_revision': revision,
    'source_tree': subprocess.check_output(['git', 'rev-parse', 'HEAD^{tree}'], text=True).strip(),
    'tracked_source_dirty': False,
    'status': 'LOCAL_WEB_FUNCTIONAL_TEST_PREPARED_WITH_BASELINE_WARNING_EXCEPTIONS',
    'strict_web_build': 'FAIL',
    'baseline_warning_exceptions': [
        'Exact unchanged stable_order_key.hpp header: force-include with -Wsign-compare isolated by diagnostic push/pop',
        'Exact unchanged apply_plan.cpp source only: -Wno-error=dangling-else via local deferred CMake include',
    ],
    'project_source_modified': False,
    'web_interaction_entry': 'ink_playground',
    'p32_composition_reference_entry': 'axiom_canvas_demo',
    'reference_limit': 'Small reference canvas; no full Minimal Host control UI',
    'chrome_startup_smoke': json.loads((out / 'chrome-smoke-result.json').read_text()),
    'physical_status': 'PHYSICAL_PENDING',
    'windows_feedback': 'USER_REPORTED_NORMAL; see windows-user-observation/observation.json',
    'official_gate_or_phys_status_changed': False,
    'artifacts': artifacts,
}
(out / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
(out / 'build/apps/build-info.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
report = f'''# Local Chrome Web preparation — 2026-10-19

source_revision: `{revision}`. Project code unchanged. Build inputs and all served artifact hashes are in
`result.json`; exact configure arguments are in `configure-command.json`.

Windows user feedback: “经测试，各项功能正常”. The full post-test Windows logs and the
user statement are preserved in `windows-user-observation/`. No detailed per-item observations
were supplied; historical matrices and Gate/PHYS statuses were not changed.

## Build and automated checks

- Emscripten 6.0.6 / LLVM 22.1.8, locked Semantic web-wasm32 and Skia web-wasm-webgl2 release SDKs.
- `python tools/setup_build_environment.py --core --semantic --target web-wasm32 --facts-output out/p32-web/build-environment.json --status`
- `python tools/setup_build_environment.py --semantic --target web-wasm32 --offline --facts-output out/p32-web/build-environment-offline.json --status`
- `python tools/skia/fetch.py --target web-wasm-webgl2 --variant release --lock r1-full-skia-sdk.lock.json --profile tools/skia/profiles/r1-full-v1.json --summary-file out/p32-web/skia-fetch.json`
- `python .deps/emsdk/emsdk.py install 6.0.6` and `activate 6.0.6`; compiler installed, SDK libraries not rebuilt.
- `python out/p32-web/check_sources.py`: 8 source contract cases PASS.
- `cmd.exe /d /c out\\p32-web\\native_checks.cmd`: G3WebSurfaceHost PASS (1 CTest).
- `cmd.exe /d /c out\\p32-web\\web_build.cmd --local-baseline-filter`: two Web targets linked.
- `python out/p32-web/smoke.py`: Chrome CLI startup status {result['chrome_startup_smoke']['status']}; no physical input inferred.
- HTTP probes: 7 exact artifact byte/hash matches, WASM MIME/magic verified, Cache-Control no-store.

## Strict build failures and local test exceptions

`strict-build-FAIL.log` preserves the unfiltered -Werror failure at unchanged
`runtime/foundation/include/canvas/foundation/stable_order_key.hpp:29` (-Wsign-compare).
`header-filter-build-FAIL.log` preserves the subsequent unchanged
`runtime/semantic/src/apply_plan.cpp:22` (-Wdangling-else) failure.
The local functional-test build isolates the former exact header using diagnostic push/pop and
applies -Wno-error=dangling-else to only the latter exact source through a deferred CMake include.
The project warning policy and sources are unchanged. Strict build result remains FAIL;
the local test consumer does not establish a clean production build.
The reference bridge also emits existing deprecation warnings (see build.log).

## Browser entries and remaining observations

Interactive entry: http://127.0.0.1:8765/ink_playground/ — existing Ink Playground with Debug UI button.
P32 RuntimeComposition reference: http://127.0.0.1:8765/axiom_canvas_demo/ — small canvas without the
full Windows Minimal Host controls. These entry points are distinguished explicitly.
Browser executable, version, PID, profile and server PID are recorded in `browser-process.json`.
The loopback server is a tracked foreground CLI session. No computer use was performed.
All Web manual items remain PHYSICAL_PENDING in `manual-matrix.md`, including any tool whose
availability in this Web UI has not been observed. Original AutoIntent semantics remain frozen:
pending second contact inside the 250 ms window enters navigation; later contact enters another Ink;
the existing first-contact Ink slop rule is preserved.
No official Gate, Control Review, true-device acceptance or PHYS PASS is claimed.
'''
(out / 'REPORT.md').write_text(report, encoding='utf-8')
for name in (
    'check_sources.py', 'configure.py', 'web_build.cmd', 'native_checks.cmd', 'serve.py', 'smoke.py', 'capture.py',
    'local-diagnostics.cmake', 'unchanged-foundation-diagnostic.hpp', 'manual-matrix.md',
    'configure-command.json', 'strict-configure-command.json', 'toolchain.txt',
    'build-environment.json', 'build-environment-offline.json', 'skia-fetch.json', 'skia-fetch-summary.md',
    'emsdk-activate.log', 'source-checks.json', 'source-checks.log', 'configure.log', 'build.log',
    'strict-build-FAIL.log', 'header-filter-build-FAIL.log',
    'native-web-surface-build.log', 'native-web-surface-tests.log',
    'chrome-smoke-command.json', 'chrome-smoke-stderr.log', 'chrome-smoke-result.json',
    'browser-process.json', 'result.json', 'REPORT.md',
):
    shutil.copy2(out / name, destination / name)
shutil.copytree(out / 'windows-user-observation', destination / 'windows-user-observation', dirs_exist_ok=True)
previous_facts = root / '.aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/windows-touch-20261019/facts'
for name in ('machine.json', 'displays.json'):
    shutil.copy2(previous_facts / name, destination / ('same-local-device-' + name))
for source, name in (
    (root / 'out/p32-web-semantic-resolve.log', 'semantic-resolve.log'),
    (root / 'out/p32-web-semantic-offline.log', 'semantic-offline.log'),
    (root / 'out/p32-web-skia-fetch.log', 'skia-fetch.log'),
    (root / 'out/p32-web-emsdk-install.log', 'emsdk-install.log'),
    (root / 'out/p32-touch/web-surface-tests.xml', 'native-web-surface-tests.xml'),
):
    shutil.copy2(source, destination / name)
print(json.dumps({'source_revision': revision, 'artifacts_verified': len(artifacts),
                  'evidence': str(destination)}, indent=2), flush=True)
