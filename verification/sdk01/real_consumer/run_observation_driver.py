#!/usr/bin/env python3
"""Run a real SDK observation driver and compare to a pinned independent oracle.

A matching comparison is not a product PASS without driver provenance review.
"""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import subprocess
import sys
import tempfile
import check_observations


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--driver', required=True, type=Path)
    parser.add_argument('--driver-sha256')
    parser.add_argument('--expected', required=True, type=Path)
    parser.add_argument('--expected-sha256', required=True)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--timeout-seconds', type=int, default=60)
    parser.add_argument('driver_args', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    result = {'status': 'BLOCKED_INPUT', 'platform': platform.platform(),
              'note': 'Not a product PASS: verify actual SUT, driver provenance, and raw platform evidence.'}
    def finish(code):
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print(result['status'])
        return code
    if not args.driver.is_file():
        result['reason'] = 'real driver executable missing'
        return finish(2)
    if not args.expected.is_file():
        result['reason'] = 'pinned expected file missing'
        return finish(2)
    if sha(args.expected) != args.expected_sha256.lower():
        result['reason'] = 'expected hash mismatch'
        return finish(2)
    driver_hash = sha(args.driver)
    if not args.driver_sha256 or driver_hash != args.driver_sha256.lower():
        result['reason'] = 'driver hash absent or mismatch'
        return finish(2)
    if not 1 <= args.timeout_seconds <= 60:
        result['reason'] = 'timeout must be between 1 and 60 seconds'
        return finish(2)
    result['inputs'] = {'driver': str(args.driver.resolve()), 'driver_sha256': driver_hash,
                        'expected': str(args.expected.resolve()), 'expected_sha256': args.expected_sha256.lower()}
    try:
        expected = check_observations.parse_json(args.expected.read_text(encoding='utf-8'))
        if not isinstance(expected, dict) or expected.get('schema_version') != 1 or not expected.get('scenarios'):
            raise ValueError('invalid expected scenarios')
        with tempfile.TemporaryDirectory(prefix='axiom-sdk01-driver-') as directory:
            observed = Path(directory) / 'observations.json'
            extra = args.driver_args[1:] if args.driver_args and args.driver_args[0] == '--' else args.driver_args
            command = [str(args.driver.resolve()), *extra, '--output', str(observed)]
            result['command'] = command
            try:
                run = subprocess.run(command, capture_output=True, text=True, check=False, timeout=args.timeout_seconds)
            except subprocess.TimeoutExpired as exc:
                result.update(status='BLOCKED_TIMEOUT', reason=str(exc))
                return finish(2)
            result['driver'] = {'exit_code': run.returncode, 'stdout': run.stdout, 'stderr': run.stderr}
            if run.returncode:
                result['status'] = 'DRIVER_FAILED'
                return finish(1)
            if not observed.is_file():
                result.update(status='FAILED_EVIDENCE', reason='driver produced no observation file')
                return finish(1)
            result['observations_sha256'] = sha(observed)
            result['observations_raw'] = observed.read_text(encoding='utf-8')
            actual = check_observations.parse_json(result['observations_raw'])
            errors = check_observations.validate(expected, actual)
            result['oracle_errors'] = errors
            result['status'] = 'ORACLE_MISMATCH' if errors else 'MATCH_REQUIRES_PROVENANCE_REVIEW'
            return finish(1 if errors else 0)
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
        result.update(status='BLOCKED_INPUT', reason=str(exc))
        return finish(2)


if __name__ == '__main__':
    sys.exit(main())
