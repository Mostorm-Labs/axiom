from pathlib import Path
import json
import importlib.util
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MemoryProfileTests(unittest.TestCase):
    def configure(self, definitions=(), system=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = '' if system is None else f'set(CMAKE_SYSTEM_NAME "{system}")\n'
            source += f'include("{ROOT}/cmake/AxiomRenderMemoryBudget.cmake")\n'
            source += 'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/effective.json" "[${AXIOM_RENDER_CACHE_MIB},${AXIOM_RENDER_SHARED_MIB},${AXIOM_RENDER_ENTRY_MIB}]")\n'
            script = root / 'profile.cmake'
            script.write_text(source)
            cp = subprocess.run(['cmake', *definitions, '-P', str(script)], cwd=root, capture_output=True, text=True)
            output = root / 'effective.json'
            return cp, json.loads(output.read_text()) if output.exists() else None

    def test_desktop_default_and_native_mobile_conservative(self):
        for system, expected in [(None, [256, 512, 64]), ('Emscripten', [256, 512, 64]),
                                 ('Android', [32, 96, 8]), ('iOS', [32, 96, 8])]:
            with self.subTest(system=system):
                cp, actual = self.configure(system=system)
                self.assertEqual(cp.returncode, 0, cp.stderr)
                self.assertEqual(actual, expected)

    def test_explicit_conservative_and_legacy_cache_override(self):
        cp, values = self.configure(['-DAXIOM_RENDER_MEMORY_PROFILE=CONSERVATIVE'])
        self.assertEqual(cp.returncode, 0, cp.stderr)
        self.assertEqual(values, [32, 96, 8])
        cp, values = self.configure(['-DAXIOM_RENDER_CACHE_MIB=32', '-DAXIOM_RENDER_SHARED_MIB=96'])
        self.assertEqual(cp.returncode, 0, cp.stderr)
        self.assertEqual(values, [32, 96, 32])
        cp, values = self.configure(['-DAXIOM_RENDER_CACHE_MIB=3', '-DAXIOM_RENDER_SHARED_MIB=5'])
        self.assertEqual(cp.returncode, 0, cp.stderr)
        self.assertEqual(values, [3, 5, 3])

    def test_rejects_invalid_or_incoherent_explicit_values(self):
        for key in ['CACHE', 'SHARED', 'ENTRY']:
            for value in ['0', '-1', '1.5', 'abc', '4097', '1;2']:
                with self.subTest(key=key, value=value):
                    cp, _ = self.configure([f'-DAXIOM_RENDER_{key}_MIB={value}'])
                    self.assertNotEqual(cp.returncode, 0)
        for flags in [
            ['-DAXIOM_RENDER_MEMORY_PROFILE=unknown'],
            ['-DAXIOM_RENDER_CACHE_MIB=513'],
            ['-DAXIOM_RENDER_CACHE_MIB=32', '-DAXIOM_RENDER_ENTRY_MIB=64'],
            ['-DAXIOM_RENDER_SHARED_MIB=32'],
        ]:
            with self.subTest(flags=flags):
                cp, _ = self.configure(flags)
                self.assertNotEqual(cp.returncode, 0)


class BudgetWindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        p = ROOT / 'verification/runners/g5/desktop_render_budget_runner.py'
        spec = importlib.util.spec_from_file_location('memory_window', p)
        cls.runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.runner)

    def test_no_retry_foreign_id_same_sdk_or_incomplete_matrix(self):
        m = self.runner
        request = {'id': m.WINDOW, 'variants': [{'name': 'old', 'sdk': '/old'}, {'name': 'candidate', 'sdk': '/candidate'}]}
        ledger = {'attempts': []}
        m.reserve(ledger, request)
        with self.assertRaises(ValueError):
            m.reserve(ledger, request)
        for patch in [{'id': 'smoke'}, {'variants': [request['variants'][0]]},
                      {'variants': [{'name': 'old', 'sdk': '/old'}, {'name': 'candidate', 'sdk': '/old'}]}]:
            with self.assertRaises(ValueError):
                m.reserve({'attempts': []}, request | patch)
        guard = {'reason': 'completed', 'returncode': 0, 'cleanup_complete': True, 'remaining_owned_pids': []}
        worker = {'status': 'OBSERVED', 'errors': [], 'rows': [
            {'width': w, 'kind': k, 'variant': v, 'status': 'OBSERVED', 'errors': [], 'roundtrip': {'status': 'QUALIFIED'}}
            for w in (600, 1431) for k in ('shape', 'stroke') for v in ('old', 'candidate')]}
        self.assertEqual(m.classify(guard, worker, True), 'COMPLETED')
        self.assertEqual(m.classify(guard, worker, False), 'STOPPED')
        self.assertEqual(m.classify(guard | {'reason': 'rss_limit'}, worker, True), 'STOPPED')
        self.assertEqual(m.classify(guard, worker | {'rows': worker['rows'][:-1]}, True), 'STOPPED')
        self.assertEqual(m.charge({'elapsed_s': 163.2}), 164)
        self.assertEqual(m.charge({'elapsed_s': float('nan')}), 300)


if __name__ == '__main__':
    unittest.main()
