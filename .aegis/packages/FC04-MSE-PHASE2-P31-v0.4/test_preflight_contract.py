#!/usr/bin/env python3
"""Negative tests for package checks; not product behavior tests."""
import copy
import importlib.util
import pathlib
import unittest
from unittest import mock

P=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('fc04_mse_preflight',P/'preflight.py')
preflight=importlib.util.module_from_spec(spec)
spec.loader.exec_module(preflight)

class PreflightContractTests(unittest.TestCase):
    def test_missing_expected_result_is_rejected(self):
        matrix=preflight.read('test-matrix.json')
        del matrix['test_cases'][6]['expected']
        with mock.patch.object(preflight,'read',return_value=matrix):
            with self.assertRaisesRegex(ValueError,'MSE-07 missing expected'):
                preflight.evidence()

    def test_wrong_case_selector_is_rejected(self):
        matrix=preflight.read('test-matrix.json')
        matrix['test_cases'][18]['test_selector']='MultiSelectionMSE.MSE_01'
        with mock.patch.object(preflight,'read',return_value=matrix):
            with self.assertRaisesRegex(ValueError,'MSE-19 selector mismatch'):
                preflight.evidence()

    def test_missing_composition_build_entry_is_rejected(self):
        context=preflight.read('implementation-context.json')
        context['allowed_modification_paths'].remove('runtime/composition/CMakeLists.txt')
        original=preflight.read
        with mock.patch.object(preflight,'read',side_effect=lambda name: context if name=='implementation-context.json' else original(name)):
            with self.assertRaisesRegex(ValueError,'required source out of scope'):
                preflight.scope()

    def test_worktree_bytes_cannot_differ_from_locked_cursor(self):
        entry=preflight.read('source-lock.json')['files'][0]
        original=pathlib.Path.read_bytes
        def altered(path):
            if path == preflight.ROOT/entry['path']:
                return b'tampered'
            return original(path)
        with mock.patch.object(pathlib.Path,'read_bytes',altered):
            with self.assertRaisesRegex(ValueError,'worktree differs from baseline'):
                preflight.verify_locked_file(entry,preflight.git('rev-parse','HEAD'))

if __name__=='__main__':unittest.main(verbosity=2)
