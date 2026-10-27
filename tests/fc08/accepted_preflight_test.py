"""Package checker sensitivity controls, never Axiom product tests."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('accepted_preflight', ROOT/'tools/fc08/accepted_preflight.py')
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class AcceptedPreflightTest(unittest.TestCase):
    def test_exact_acceptance(self):
        self.assertTrue(CHECK.acceptance_matches({'accepted':True,'candidate_revision':'a'*40,'candidate_package_sha256':'b'*64,'accepts':['source_truth','profile_matrix'],'product_P32_authorized':False}, 'a'*40,'b'*64))

    def test_other_revision_does_not_accept(self):
        self.assertFalse(CHECK.acceptance_matches({'accepted':True,'candidate_revision':'c'*40,'candidate_package_sha256':'b'*64,'accepts':['source_truth','profile_matrix'],'product_P32_authorized':False}, 'a'*40,'b'*64))

    def test_truth_only_does_not_accept_matrix(self):
        self.assertFalse(CHECK.acceptance_matches({'accepted':True,'candidate_revision':'a'*40,'candidate_package_sha256':'b'*64,'accepts':['source_truth'],'product_P32_authorized':False}, 'a'*40,'b'*64))

    def test_product_permission_is_not_inherited(self):
        self.assertFalse(CHECK.acceptance_matches({'accepted':True,'candidate_revision':'a'*40,'candidate_package_sha256':'b'*64,'accepts':['source_truth','profile_matrix'],'product_P32_authorized':True}, 'a'*40,'b'*64))

    def test_path_escape_rejected(self):
        with self.assertRaises(ValueError): CHECK.safe_path(ROOT,'../../outside.json')

    def test_partial_revision_rejected(self):
        with self.assertRaises(ValueError): CHECK.exact_revision('eb12da1a')


if __name__ == '__main__': unittest.main()
