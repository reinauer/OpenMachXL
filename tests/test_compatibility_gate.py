"""The archive gate must distinguish equivalent logic from identical fitting."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_compatibility import case_matches, error_codes, summarize, validate_case_set


class CompatibilityGateTests(unittest.TestCase):
    def setUp(self):
        self.case = {"original_fit_succeeded": True,
                     "pla": {"comparison": {"equivalent": True, "matrix_identical": True}},
                     "jedec": {"returncode": 0, "comparison": {"equal": True, "bytes_equal": False}}}

    def test_equivalent_functions_are_not_exact_translation(self):
        self.case["pla"]["comparison"]["matrix_identical"] = False
        self.assertFalse(case_matches(self.case))

    def test_native_rejection_does_not_reproduce_success(self):
        self.case["jedec"] = {"returncode": 1, "stderr": "unsupported device"}
        self.assertFalse(case_matches(self.case))

    def test_original_failure_requires_its_error(self):
        self.case["original_fit_succeeded"] = False
        self.case["original_error_codes"] = ["z8007"]
        self.case["jedec"] = {"returncode": 1, "stderr": "unsupported device"}
        self.assertFalse(case_matches(self.case))
        self.case["jedec"]["stderr"] = "z8007 partition failed"
        self.assertTrue(case_matches(self.case))
        self.case["jedec"]["partial_output_on_failure"] = True
        self.assertFalse(case_matches(self.case))

    def test_matching_bits_do_not_imply_matching_bytes_or_all_language(self):
        report = summarize([self.case])
        self.assertTrue(report["all_archive_results_match"])
        self.assertEqual(report["fuse_configuration_matches"], 1)
        self.assertEqual(report["exact_jedec_file_matches"], 0)
        self.assertFalse(report["complete_compiler_verified"])
        self.assertFalse(summarize([])["all_archive_results_match"])

    def test_original_warnings_are_not_fit_errors(self):
        self.assertEqual(error_codes("WARNING z5060 quality\nINFO z5065 settings\nERROR z8007 failed\n"), ["z8007"])

    def test_subset_cannot_pass_as_full_archive(self):
        names = ["A.PDS", "B.PDS", "A.MXL"]
        with self.assertRaises(ValueError):
            validate_case_set([{"name": "A"}], names)
        with self.assertRaises(ValueError):
            validate_case_set([{"name": "A"}, {"name": "B"}, {"name": "B"}], names)
        validate_case_set([{"name": "A"}, {"name": "B"}], names)


if __name__ == "__main__":
    unittest.main()
