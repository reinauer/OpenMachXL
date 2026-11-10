import unittest

from tools.compare_pla import PlaError, compare_pla, parse_pla


def pla(inputs, outputs, rows, phase=None, header=""):
    lines = [header, ".type f", f".i {len(inputs)}", f".o {len(outputs)}",
             ".ilb " + " ".join(inputs), ".ob " + " ".join(outputs)]
    if phase is not None:
        lines.append(".phase " + phase)
    lines += [f".p {len(rows)}", *rows, ".e"]
    return parse_pla("\n".join(lines))


class PlaTests(unittest.TestCase):
    def test_label_orders_are_mapped_by_name(self):
        a = pla(["A", "B"], ["Y", "Z"], ["10 1~", "-1 ~1"])
        b = pla(["B", "A"], ["Z", "Y"], ["01 ~1", "1- 1~"])
        result = compare_pla(a, b)
        self.assertTrue(result["equivalent"])
        self.assertEqual(result["equivalent_functions"], 2)
        self.assertFalse(result["matrix_equal"])

    def test_phase_is_applied(self):
        a = pla(["A"], ["Y"], ["1 1"], "1")
        b = pla(["A"], ["Y"], ["0 1"], "0")
        self.assertTrue(compare_pla(a, b)["equivalent"])
        wrong = pla(["A"], ["Y"], ["1 1"], "0")
        result = compare_pla(a, wrong)
        self.assertFalse(result["equivalent"])
        self.assertEqual(result["counterexamples"]["Y"], {"A": False})

    def test_coverage_equivalence_and_redundancy(self):
        a = pla(["A", "B"], ["Y"], ["1- 1"])
        b = pla(["A", "B"], ["Y"], ["10 1", "11 1", "11 1"])
        self.assertTrue(compare_pla(a, b)["equivalent"])

    def test_different_functions_and_missing_outputs_fail(self):
        a = pla(["A", "B"], ["Y"], ["1- 1"])
        b = pla(["A", "B"], ["Y"], ["-1 1"])
        result = compare_pla(a, b)
        self.assertFalse(result["equivalent"])
        self.assertEqual(result["different_outputs"], ["Y"])
        self.assertEqual(result["counterexamples"]["Y"], {"A": False, "B": True})
        missing = pla(["A", "B"], ["Z"], ["1- 1"])
        result = compare_pla(a, missing)
        self.assertEqual(result["missing_in_left"], ["Z"])
        self.assertEqual(result["missing_in_right"], ["Y"])
        self.assertFalse(result["equivalent"])

    def test_continued_labels(self):
        obj = parse_pla(".type f\n.i 3\n.o 2\n.ilb A\n B C\n.ob Y\n Z\n.p 1\n100 1~\n.e")
        self.assertEqual(obj.inputs, ("A", "B", "C"))
        self.assertEqual(obj.outputs, ("Y", "Z"))

    def test_nonmember_output_symbols_in_type_f(self):
        a = pla(["A"], ["Y"], ["1 1", "0 -"])
        for symbol in ("0", "~"):
            self.assertTrue(compare_pla(a, pla(["A"], ["Y"], ["1 1", "0 " + symbol]))["equivalent"])

    def test_constants_and_unused_inputs(self):
        true = pla([], ["Y"], ["1"])
        false = pla([], ["Y"], [])
        self.assertFalse(compare_pla(true, false)["equivalent"])
        self.assertTrue(compare_pla(true, pla(["UNUSED"], ["Y"], ["- 1"]))["equivalent"])
        self.assertTrue(compare_pla(true, pla([], ["Y"], [], "0"))["equivalent"])

    def test_device_mismatch_is_separate_from_function_match(self):
        a = pla(["A"], ["Y"], ["1 1"], header="#AMD DEVICE MACH111")
        b = pla(["A"], ["Y"], ["1 1"], header="#AMD DEVICE MACH110")
        result = compare_pla(a, b)
        self.assertTrue(result["functions_equivalent"])
        self.assertFalse(result["equivalent"])

    def test_malformed_files_fail(self):
        good = ".type f\n.i 2\n.o 1\n.ilb A B\n.ob Y\n.phase 1\n.p 1\n10 1\n.e\n"
        bad = [good.replace("10 1", "1 1"), good.replace("10 1", "10 11"),
               good.replace("10 1", "X0 1"), good.replace("10 1", "10 X"),
               good.replace(".p 1", ".p 2"), good.replace(".ilb A B", ".ilb A A"),
               good.replace(".phase 1", ".phase 11"), good.replace(".type f", ".type fd"),
               good.replace(".e\n", ""), good + "00 1\n", good.replace(".p 1", ".foo 1\n.p 1"),
               good.replace(".ob Y", ".ob"), good.replace(".i 2", ".i 2\n.i 2")]
        for text in bad:
            with self.subTest(text=text), self.assertRaises(PlaError):
                parse_pla(text)

    def test_budgets_fail_without_equivalence_claim(self):
        obj = pla(["A", "B"], ["Y"], ["10 1", "01 1"])
        with self.assertRaisesRegex(PlaError, "node budget"):
            compare_pla(obj, obj, max_nodes=0)
        with self.assertRaisesRegex(PlaError, "work budget"):
            compare_pla(obj, obj, max_work=0)


if __name__ == "__main__":
    unittest.main()
