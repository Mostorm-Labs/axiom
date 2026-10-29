#!/usr/bin/env python3
"""Install and exercise the actual SDK in a fresh directory outside the repository."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
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
    with tempfile.TemporaryDirectory(prefix='axiom-installed-consumer-') as temp:
        root = Path(temp).resolve()
        if root.is_relative_to(repo):
            raise RuntimeError('external consumer must be outside repository')
        install, source, build = root / 'install', root / 'consumer', root / 'build'
        shutil.copytree(Path(__file__).parent / 'consumer', source)
        commands = [
            ['cmake', '--install', str(args.build.resolve()), '--prefix', str(install)],
            ['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
             '-DCMAKE_BUILD_TYPE=Release', '-DAxiomRuntime_DIR=' + str(install / 'lib/cmake/AxiomRuntime'),
             '-DCMAKE_PREFIX_PATH=' + str(args.semantic_root.resolve()),
             '-DCanvasSkia_DIR=' + str(args.skia_root.resolve() / 'lib/cmake/CanvasSkia')],
            ['cmake', '--build', str(build)],
            ['ctest', '--test-dir', str(build), '-R', '^(ExternalRuntimeLifecycle|ExternalMultiStrokeStyle)$',
             '--no-tests=error', '--output-on-failure', '--output-junit', str(root / 'external-results.xml')],
        ]
        for command in commands:
            run = subprocess.run(command, text=True, capture_output=True, env=os.environ.copy(), check=False)
            result['steps'].append({'command': command, 'exit_code': run.returncode,
                                    'stdout': run.stdout, 'stderr': run.stderr})
            if run.returncode:
                result['status'] = 'FAIL'
                break
        else:
            suite = ET.parse(root / 'external-results.xml').getroot()
            cases = suite.findall('.//testcase')
            if {x.get('name') for x in cases} != {'ExternalRuntimeLifecycle', 'ExternalMultiStrokeStyle'} or len(cases) != 2 or any(
                    x.find('failure') is not None or x.find('error') is not None or x.find('skipped') is not None for x in cases):
                result['status'] = 'FAIL'
                args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
                return 1
            result['junit'] = (root / 'external-results.xml').read_text(encoding='utf-8')
            bad_crt = commands[1].copy()
            bad_crt[bad_crt.index('-B') + 1] = str(root / 'incompatible-build')
            bad_crt.append('-DAXIOM_TEST_BAD_CRT=ON')
            run = subprocess.run(bad_crt, text=True, capture_output=True, env=os.environ.copy(), check=False)
            result['steps'].append({'command': bad_crt, 'exit_code': run.returncode,
                                    'stdout': run.stdout, 'stderr': run.stderr,
                                    'expected': 'configure must reject incompatible /MD CRT'})
            result['status'] = 'PASS' if run.returncode != 0 and 'AxiomRuntime requires' in run.stderr else 'FAIL'
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'status': result['status'], 'output': str(args.output)}))
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
