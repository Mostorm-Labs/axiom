"""Run only the Windows touch repair's selected regression targets."""
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / 'out/p32-touch'
BUILD = OUT / 'build'
SELECTED = '''G4InputRouter G4MultiPointerFoundation G4PlatformNormalization
C1CommonIngress C1CrossPlatformFixtureParity G4InteractionSessionManager
G4SelectionSession G4TransformSession G46TransformHandleDrag G4EraserSession
G4SessionConflict G4SessionLifecycle G4MultiContactCoordinator G4ViewportGesture
C0ViewportInteractionController G3RuntimeAppBinding FC00XRuntimeIsolation G3HeadlessSkia
FC00XFrameCoordinator G3RenderTargetSwitching G3SkiaSurfaceProvider G3PreviewSurface
G3SurfacePresentation G3FrameState G46EditingOverlay G4CanonicalHandoff
FC00XBrushBatchEvaluation FC00XBrushPressureCapability FC00XBrushBatchSemantics
FC00XCanonicalBusyPreviewProgress FC00XPreviewBusyCanonicalProgress
FC00XBusyCanonicalPendingFeedback FC00XFatalProviderFailure FC00XRuntimeComposition
FC00XRuntimeFacade FC00XWindowsTouchIngress FC00XWindowsMouseHistory
FC00XWindowsPreviewOrigin FC00XPreviewCanonicalCoordinates FC00XS2WindowsSurfaceBinding
FC00XMinimalRuntimeHost FC00XMinimalPresentationCache DUIWindowsHostInputDispatch
PX0Workbench PX0RuntimeFacadeContract DUIM1PanelModel PX0ControlRouter PX0DebugSnapshot'''.split()
FILES = ['runtime/input/CMakeLists.txt', 'runtime/interaction/CMakeLists.txt',
         'runtime/render/CMakeLists.txt', 'runtime/composition/CMakeLists.txt',
         'apps/minimal_runtime_host/CMakeLists.txt', 'debug_ui/CMakeLists.txt']
pairs = dict(p for f in FILES for p in re.findall(
    r'add_test\(NAME\s+(\w+)\s+COMMAND\s+(\w+)', (ROOT / f).read_text()))
assert all(name in pairs for name in SELECTED)
targets = sorted({pairs[name] for name in SELECTED} | {'axiom_minimal_runtime_host_windows'})
regex = '^(' + '|'.join(SELECTED) + ')$'
commands = [['cmake', '--build', str(BUILD), '--target', *targets, '-j', '6'],
            ['ctest', '--test-dir', str(BUILD), '-R', regex, '--output-on-failure',
             '--output-junit', str(OUT / 'selected-tests.xml')]]
(OUT / 'selected-commands.json').write_text(json.dumps(commands, indent=2))
for i, command in enumerate(commands):
    print(subprocess.list2cmdline(command), flush=True)
    with (OUT / ('selected-build.log' if i == 0 else 'selected-tests.log')).open('w') as log:
        result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    print(f'command {i}: exit={result.returncode}', flush=True)
    if result.returncode: sys.exit(result.returncode)
discovered = subprocess.check_output(['ctest', '--test-dir', str(BUILD), '-R', regex,
                                     '--show-only=json-v1'], cwd=ROOT, text=True)
discovered = json.loads(discovered)
assert {t['name'] for t in discovered['tests']} == set(SELECTED)
(OUT / 'selected-tests.json').write_text(json.dumps(discovered, indent=2))
