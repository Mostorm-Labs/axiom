import copy
import importlib.util
import json
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('sdk01_oracle', HERE / 'check_observations.py')
oracle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(oracle)

class ObservationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.expected = json.loads((HERE / 'expected-v1.json').read_text(encoding='utf-8'))

    def test_duplicate_json_key_is_rejected(self):
        with self.assertRaises(ValueError):
            oracle.parse_json('{"scenarios": {}, "scenarios": {}}')

    def test_nonfinite_json_is_rejected(self):
        with self.assertRaises(ValueError):
            oracle.parse_json('{"x": NaN}')

    def test_complete_exact_observations(self):
        self.assertEqual([], oracle.validate(self.expected, copy.deepcopy(self.expected)))

    def test_empty_results_fail(self):
        self.assertTrue(oracle.validate(self.expected, {'scenarios': {}}))

    def test_missing_step_fails(self):
        actual = copy.deepcopy(self.expected)
        del actual['scenarios']['partial_fill']['steps']['undo']
        self.assertTrue(oracle.validate(self.expected, actual))

    def test_wrong_color_or_unselected_object_fails(self):
        actual = copy.deepcopy(self.expected)
        actual['scenarios']['stroke_width']['steps']['after']['objects']['shape_b']['stroke']['color'] = [1, 0, 0, 1]
        self.assertTrue(oracle.validate(self.expected, actual))
        actual = copy.deepcopy(self.expected)
        actual['scenarios']['partial_fill']['steps']['after']['objects']['image_x']['fill'] = 'red'
        self.assertTrue(oracle.validate(self.expected, actual))

    def test_wrong_history_or_receipt_fails(self):
        actual = copy.deepcopy(self.expected)
        actual['scenarios']['partial_fill']['steps']['after']['history_cursor'] = 2
        self.assertTrue(oracle.validate(self.expected, actual))
        actual = copy.deepcopy(self.expected)
        actual['scenarios']['partial_fill']['steps']['after']['receipt']['changed'] = ['shape_a']
        self.assertTrue(oracle.validate(self.expected, actual))

    def test_extra_scenarios_are_not_ignored(self):
        actual = copy.deepcopy(self.expected)
        actual['scenarios']['extra'] = {}
        self.assertTrue(oracle.validate(self.expected, actual))

    def test_type_confusion_and_nonfinite_fail(self):
        actual = copy.deepcopy(self.expected)
        actual['scenarios']['stroke_width']['steps']['after']['objects']['shape_a']['stroke']['width'] = True
        self.assertTrue(oracle.validate(self.expected, actual))
        actual = copy.deepcopy(self.expected)
        actual['scenarios']['stroke_width']['steps']['after']['objects']['shape_a']['stroke']['width'] = float('nan')
        self.assertTrue(oracle.validate(self.expected, actual))

if __name__ == '__main__':
    unittest.main()
