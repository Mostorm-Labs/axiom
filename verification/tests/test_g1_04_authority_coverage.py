import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("coverage", ROOT / "tools/check_g1_04_a_authority_coverage.py")
assert SPEC and SPEC.loader
coverage = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(coverage)


HEADER = "| " + " | ".join(coverage.HEADERS) + " |\n"
SEPARATOR = "| " + " | ".join(["---"] * len(coverage.HEADERS)) + " |\n"


def row(rule_id, owner, status):
    return f"| {rule_id} | Authority | Rule | {owner} | Carrier | impl | oracle | {status} |\n"


class G104AuthorityCoverageTest(unittest.TestCase):
    def test_current_matrix_parses_and_counts_multi_owner_rows(self):
        summary = coverage.summarize(coverage.parse_matrix((ROOT / "docs/planning/GT_G1_04_A_AUTHORITY_COVERAGE_MATRIX.md").read_text()))
        self.assertFalse(summary["errors"])
        self.assertGreater(summary["ownerMemberships"]["A3"], summary["ownerMemberships"]["A0"])
        self.assertGreater(summary["normativeRuleRows"], summary["coveredARows"])

    def test_lineage_is_not_counted(self):
        text = "# Matrix\n\n" + HEADER + SEPARATOR + row("A", "A1/A3", "COVERED") + "\n## P34 Review Finding Lineage\n\n" + HEADER + SEPARATOR + row("LINEAGE", "A0", "MISSING")
        summary = coverage.summarize(coverage.parse_matrix(text))
        self.assertEqual(summary["normativeRuleRows"], 1)
        self.assertEqual(summary["ownerMemberships"], {"A0": 0, "A1": 1, "A2": 0, "A3": 1})

    def test_a_owned_bad_statuses_fail(self):
        for status in ("MISSING", "WRONG_ORACLE", "UNOWNED", "BLOCKED_CARRIER"):
            summary = coverage.summarize(coverage.parse_matrix("# Matrix\n\n" + HEADER + SEPARATOR + row("A", "A2", status)))
            self.assertTrue(summary["errors"], status)

    def test_only_genuine_b_and_c_deferrals_are_allowed(self):
        text = "# Matrix\n\n" + HEADER + SEPARATOR + row("B", "B", "DEFERRED_B") + row("C", "C", "DEFERRED_C")
        summary = coverage.summarize(coverage.parse_matrix(text))
        self.assertFalse(summary["errors"])
        self.assertEqual(summary["deferredBRows"], 1)
        self.assertEqual(summary["deferredCRows"], 1)


if __name__ == "__main__":
    unittest.main()
