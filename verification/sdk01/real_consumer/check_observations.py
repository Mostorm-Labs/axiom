#!/usr/bin/env python3
"""Compare independently specified observations; this alone is not product evidence."""
import argparse
import json
import math
import sys
from pathlib import Path


def parse_json(text):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'duplicate key: {key}')
            result[key] = value
        return result
    def reject_constant(value):
        raise ValueError(f'nonfinite JSON value: {value}')
    return json.loads(text, object_pairs_hook=unique, parse_constant=reject_constant)


def validate(expected, actual):
    errors = []
    def compare(want, got, path):
        if len(errors) >= 100:
            return
        if isinstance(want, dict):
            if not isinstance(got, dict):
                errors.append(f'{path}: expected object')
                return
            for key in sorted(want.keys() - got.keys()):
                errors.append(f'{path}.{key}: missing')
            for key in sorted(got.keys() - want.keys()):
                errors.append(f'{path}.{key}: unexpected')
            for key in sorted(want.keys() & got.keys()):
                compare(want[key], got[key], f'{path}.{key}')
        elif isinstance(want, list):
            if not isinstance(got, list):
                errors.append(f'{path}: expected array')
            elif len(want) != len(got):
                errors.append(f'{path}: expected {len(want)} elements, got {len(got)}')
            else:
                for i, (w, g) in enumerate(zip(want, got)):
                    compare(w, g, f'{path}[{i}]')
        elif isinstance(want, (int, float)) and not isinstance(want, bool):
            if isinstance(got, bool) or not isinstance(got, (int, float)) or not math.isfinite(got) or got != want:
                errors.append(f'{path}: expected {want!r}, got {got!r}')
        elif type(want) is not type(got) or want != got:
            errors.append(f'{path}: expected {want!r}, got {got!r}')
    compare(expected, actual, '$')
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--expected', type=Path, required=True)
    parser.add_argument('--observed', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        expected = parse_json(args.expected.read_text(encoding='utf-8'))
        actual = parse_json(args.observed.read_text(encoding='utf-8'))
        if not isinstance(expected, dict) or expected.get('schema_version') != 1 or not isinstance(expected.get('scenarios'), dict) or not expected['scenarios']:
            raise ValueError('empty or unsupported expected scenarios')
        errors = validate(expected, actual)
        result = {'status': 'ORACLE_MATCH_ONLY' if not errors else 'MISMATCH',
                  'errors': errors, 'note': 'Comparison only; provenance, real SUT, and platform execution require separate evidence.'}
        code = 0 if not errors else 1
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
        result = {'status': 'BLOCKED_INPUT', 'error': str(exc)}
        code = 2
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(result['status'])
    return code


if __name__ == '__main__':
    sys.exit(main())
