#!/usr/bin/env python3
"""Producer-only Ninja launcher: stable clang-cl source spelling, no shell.

Clang's Microsoft ABI hashes the *input filename* for anonymous namespaces.
Prefix maps do not change that hash. Keep the producer's sources/build layout
fixed and pass each source relative to its build directory instead. No global
junction, drive mapping, shared staging directory or source mutation is used.
"""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


_SOURCE_SUFFIXES = {'.c', '.cc', '.cpp', '.cxx', '.c++'}


def relative_compile_command(command: list[str], cwd: Path) -> list[str]:
    if not command:
        raise ValueError('semantic: missing compiler command')
    if any(arg.startswith('@') for arg in command[1:]):
        raise ValueError('semantic: opaque compiler response files are not supported')
    cwd = cwd.resolve(strict=True)
    result = list(command)
    sources = 0
    for index, arg in enumerate(command[1:], 1):
        path = Path(arg)
        if path.suffix.lower() not in _SOURCE_SUFFIXES:
            continue
        path = path if path.is_absolute() else cwd / path
        if not path.is_file():
            continue
        # relpath also fails closed for sources on a different Windows drive.
        result[index] = Path(os.path.relpath(path.resolve(strict=True), cwd)).as_posix()
        sources += 1
    if sources != 1:
        raise ValueError('semantic: expected exactly one explicit compiler source input')
    return result


def main() -> int:
    try:
        command = relative_compile_command(sys.argv[1:], Path.cwd())
    except (ValueError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 2
    return subprocess.run(command, check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
