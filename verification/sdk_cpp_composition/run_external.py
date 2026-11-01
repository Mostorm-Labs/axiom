#!/usr/bin/env python3
"""Run the public single-Composition contract outside the source checkout."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--semantic-root', type=Path, required=True)
    parser.add_argument('--skia-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    result = {'status': 'NOT_RUN', 'steps': []}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='axiom-composition-consumer-') as temp:
        root = Path(temp).resolve()
        if root.is_relative_to(repo):
            raise RuntimeError('consumer must be outside repository')
        install, source, build = root / 'install', root / 'source', root / 'build'
        shutil.copytree(Path(__file__).parent / 'consumer', source)
        commands = [
            ['cmake', '--install', str(args.build.resolve()), '--prefix', str(install)],
            ['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
             '-DCMAKE_BUILD_TYPE=Release', '-DAxiomRuntime_DIR=' + str(install / 'lib/cmake/AxiomRuntime'),
             '-DCMAKE_PREFIX_PATH=' + str(args.semantic_root.resolve()),
             '-DCanvasSkia_DIR=' + str(args.skia_root.resolve() / 'lib/cmake/CanvasSkia')],
            ['cmake', '--build', str(build)],
            ['ctest', '--test-dir', str(build), '-R', '^ExternalCompositionSingleOwner$',
             '--no-tests=error', '--output-on-failure', '--output-junit', str(root / 'results.xml')],
        ]
        for command in commands:
            run = subprocess.run(command, text=True, capture_output=True, env=os.environ.copy(), check=False)
            result['steps'].append({'command': command, 'exit_code': run.returncode,
                                    'stdout': run.stdout, 'stderr': run.stderr})
            if run.returncode:
                result['status'] = 'FAIL'
                break
            if command[1] == '--install' and (install / 'include/canvas/runtime/runtime.hpp').exists():
                result['status'] = 'FAIL_LEGACY_PUBLIC_RUNTIME_ENTRY'
                break
            if command[1] == '--build':
                output = (run.stdout + run.stderr).replace('\\', '/').lower()
                forbidden = ['/canvas/interaction/', '/canvas/scene/', '/canvas/render/',
                             '/canvas/runtime/internal/', '/include/core/sk']
                leaked = [item for item in forbidden if item in output]
                if leaked:
                    result.update(status='FAIL_PUBLIC_HEADER_LEAK', forbidden=leaked)
                    break
        else:
            suite = ET.parse(root / 'results.xml').getroot()
            cases = suite.findall('.//testcase')
            if len(cases) == 1 and cases[0].get('name') == 'ExternalCompositionSingleOwner' and not any(
                    cases[0].find(x) is not None for x in ('failure', 'error', 'skipped')):
                result['status'] = 'PASS_SCOPED'
                result['junit'] = (root / 'results.xml').read_text(encoding='utf-8')
            else:
                result['status'] = 'FAIL_TEST_EVIDENCE'
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'status': result['status'], 'output': str(args.output)}))
    return 0 if result['status'] == 'PASS_SCOPED' else 1


if __name__ == '__main__':
    raise SystemExit(main())
