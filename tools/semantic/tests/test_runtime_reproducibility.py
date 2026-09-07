"""Regression coverage for SEM-SDK-V2-WIN-REPRO-01 using actual COFF objects."""
import copy
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

from tools.sdk.model import SdkError
from tools.semantic.build_runtime import _prefix_maps
from tools.semantic.contract import (
    HOST_KEYS, RUNTIME_KEYS, ROOT, load_profile, make_host_tool_identity,
    make_runtime_identity, validate_profile,
)
from tools.semantic.toolchain import target_cmake_arguments
from tools.semantic.package_runtime import package_runtime
from tools.semantic.tests.test_contract_v2 import fixture_toolchain
from tools.semantic.tests.test_package_runtime_v2 import runtime_fixture

WINDOWS = 'windows-x64-msvc-static'


class WindowsProducerIdentityTest(unittest.TestCase):
    def test_only_windows_advances_to_runtime_contract_two(self):
        lock = json.loads((ROOT / 'deps.lock.json').read_bytes())
        profile = load_profile()
        previous = copy.deepcopy(profile)
        previous.pop('runtimeContractVersions', None)
        for key in RUNTIME_KEYS:
            before = make_runtime_identity(lock, previous, key, fixture_toolchain())
            after = make_runtime_identity(lock, profile, key, fixture_toolchain())
            self.assertEqual(after[0]['contractVersion'], 2 if key == WINDOWS else 1)
            self.assertEqual(before == after, key != WINDOWS)
        for key in HOST_KEYS:
            self.assertEqual(make_host_tool_identity(lock, previous, key),
                             make_host_tool_identity(lock, profile, key))

    def test_invalid_per_target_contract_versions_fail(self):
        for versions in ({'typo': 2}, {WINDOWS: 0}, {WINDOWS: True}, {WINDOWS: '2'}, []):
            with self.subTest(versions=versions), self.assertRaises(SdkError):
                validate_profile({**load_profile(), 'runtimeContractVersions': versions})


class WindowsCompilerPlanTest(unittest.TestCase):
    def test_both_languages_receive_brepro_and_launcher(self):
        args = _prefix_maps(target_cmake_arguments(load_profile(), WINDOWS),
                            Path('/fixture/sources'), Path('/fixture/build'), True)
        for language in ('C', 'CXX'):
            flags = next(a for a in args if a.startswith(f'-DCMAKE_{language}_FLAGS='))
            self.assertIn('/Brepro', flags)
            self.assertTrue(any(a.startswith(f'-DCMAKE_{language}_COMPILER_LAUNCHER=') for a in args))

    def test_non_windows_does_not_receive_windows_launcher(self):
        args = _prefix_maps([], Path('/sources'), Path('/build'), False)
        self.assertFalse(any('LAUNCHER' in a or '/Brepro' in a for a in args))


@unittest.skipUnless(shutil.which('clang-cl'), 'requires a real clang-cl COFF compiler')
class WindowsCoffReproducibilityTest(unittest.TestCase):
    def compile_object(self, root: Path, language: str, *, header=False) -> bytes:
        source = root / 'sources/protobuf/source'
        source.mkdir(parents=True)
        build = root / 'build/protobuf'
        build.mkdir(parents=True)
        file = source / ('probe.cc' if language == 'CXX' else 'probe.c')
        file.write_text(
            'namespace { volatile int value=7; int get(){return value;} }\n'
            'extern "C" int probe(){return get();}\nconst char *name=__FILE__;\n'
            if language == 'CXX' else 'int probe(void){return 7;}\n', encoding='utf-8')
        install = root / 'install/include'
        install.mkdir(parents=True)
        if header:
            (install / 'location.h').write_text('const char *header_name=__FILE__;\n')
            file.write_text('#include "location.h"\n' + file.read_text())
        args = _prefix_maps(target_cmake_arguments(load_profile(), WINDOWS),
                            root / 'sources', root / 'build', True, install_root=root / 'install')
        flags = next(a.split('=', 1)[1] for a in args if a.startswith(f'-DCMAKE_{language}_FLAGS='))
        launcher = next((a.split('=', 1)[1].split(';') for a in args
                         if a.startswith(f'-DCMAKE_{language}_COMPILER_LAUNCHER=')), [])
        command = [*launcher, shutil.which('clang-cl'), '/nologo', '/c',
                   *shlex.split(flags), '/I' + str(install.resolve()), str(file.resolve()), '/Foprobe.obj']
        result = subprocess.run(command, cwd=build, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return (build / 'probe.obj').read_bytes()

    def test_anonymous_namespace_coff_is_identical_across_distinct_roots(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            a = self.compile_object(base / 'one random root', 'CXX')
            b = self.compile_object(base / 'different/deeper second root', 'CXX')
            self.assertEqual(hashlib.sha256(a).hexdigest(), hashlib.sha256(b).hexdigest())

    def test_installed_header_file_macros_are_independent_of_install_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            a = self.compile_object(base / 'first', 'CXX', header=True)
            b = self.compile_object(base / 'another/deep path', 'CXX', header=True)
            self.assertEqual(hashlib.sha256(a).hexdigest(), hashlib.sha256(b).hexdigest())

    @unittest.skipIf(os.name == 'nt', 'the Windows tempfile root already exercises its short-path alias')
    def test_installed_header_prefix_is_canonical_through_directory_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            actual = Path(directory) / 'real root'
            actual.mkdir()
            alias = Path(directory) / 'alias'
            alias.symlink_to(actual, target_is_directory=True)
            a = self.compile_object(alias / 'first', 'CXX', header=True)
            b = self.compile_object(alias / 'different/deeper root', 'CXX', header=True)
            self.assertEqual(hashlib.sha256(a).hexdigest(), hashlib.sha256(b).hexdigest(),
                             repr([re.findall(rb'[ -~]{8,}', value) for value in (a, b)]))

    def test_c_coff_has_no_wall_clock_timestamp(self):
        with tempfile.TemporaryDirectory() as directory:
            coff = self.compile_object(Path(directory), 'C')
            self.assertEqual(struct.unpack_from('<I', coff, 4)[0], 0,
                             'C object timestamp must not depend on the compile wall clock')


class WindowsCompileLauncherTest(unittest.TestCase):
    def test_only_source_argument_changes_and_spaces_survive(self):
        from tools.semantic.windows_compile import relative_compile_command
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / 'sources/a path/probe.cc'
            source.parent.mkdir(parents=True)
            source.write_text('int f();')
            build = base / 'build/abseil'
            build.mkdir(parents=True)
            command = ['clang-cl', '/c', '/I' + str(source.parent), '/DNAME=hello world',
                       str(source), '/Foobject.obj']
            result = relative_compile_command(command, build)
            self.assertEqual(result, [*command[:4], '../../sources/a path/probe.cc', command[-1]])
            self.assertEqual(command[-2], str(source))

    def test_opaque_response_files_are_not_silently_accepted(self):
        from tools.semantic.windows_compile import relative_compile_command
        with self.assertRaisesRegex(ValueError, 'response'):
            relative_compile_command(['clang-cl', '@source.rsp'], Path.cwd())

    def test_compiler_failure_is_propagated(self):
        launcher = ROOT / 'tools/semantic/windows_compile.py'
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'x.c'
            source.write_text('int x;')
            result = subprocess.run([sys.executable, str(launcher), sys.executable,
                                     '-c', 'raise SystemExit(7)', str(source)],
                                    cwd=directory, capture_output=True, text=True)
            self.assertEqual(result.returncode, 7, result.stderr)


class CleanRuntimeComparisonTest(unittest.TestCase):
    def test_distinct_valid_identical_packages_match(self):
        from tools.semantic.qualify_runtime import compare_runtime_packages
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            lock = json.loads((ROOT / 'deps.lock.json').read_bytes())
            records = []
            for label in ('one', 'two'):
                package = base / label / 'package'
                record = package_runtime(lock, load_profile(), WINDOWS,
                                         runtime_fixture(base / label / 'install', True),
                                         fixture_toolchain(), package)
                (package / 'record.json').write_text(json.dumps(record))
                records.append(package)
            facts = compare_runtime_packages(*records)
            self.assertTrue(facts['byteIdentical'])
            self.assertEqual(facts['runtimeId'], record['runtimeId'])
            self.assertEqual(facts['sha256'], record['sha256'])
            with self.assertRaises(SdkError):
                compare_runtime_packages(records[0], records[0])
            (records[1] / record['asset']).write_bytes(b'changed archive')
            with self.assertRaises(SdkError):
                compare_runtime_packages(*records)


if __name__ == '__main__':
    unittest.main()
