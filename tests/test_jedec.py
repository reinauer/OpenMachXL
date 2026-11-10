"""Independent checksum vectors and malformed-file checks for tools.jedec."""

import unittest

from tools.jedec import JedecError, compare, parse


def frame(body, checksum=True):
    data = b"\x02" + body.encode("ascii") + b"\x03"
    return data + (f"{sum(data) & 0xffff:04X}".encode() if checksum else b"0000")


class JedecTests(unittest.TestCase):
    def test_sparse_and_explicit_maps_match(self):
        # Bits 0, 7 and 8 -> packed bytes 0x81 + 0x01 = 0x82.
        a = parse(frame("first header*QF9*F0*L0 1*L7 11*C0082*G0*N old date*"))
        b = parse(frame("second header*QF9*L0 100000011*C0082*G0*N new date*"))
        result = compare(a, b)
        self.assertTrue(result["equal"])
        self.assertFalse(result["bytes_equal"])
        self.assertEqual(a.fuse_checksum, 0x82)

    def test_one_default_and_partial_byte_padding(self):
        # QF9 F1 -> 0xff + 0x01, minus cleared bit zero = 0xff.
        obj = parse(frame("QF9*F1*L0 0*C00FF*"))
        self.assertEqual(obj.fuses, bytes([0] + [1] * 8))

    def test_electrical_fuses_continue_without_byte_padding(self):
        # Four normal fuses plus eight E fuses -> 0xF0 + 0x0F = 0xFF.
        obj = parse(frame("MACH111\nDESIGN*QF4*F0*E11111111*C00FF*"))
        self.assertEqual(obj.device, "MACH111")
        self.assertEqual(obj.electrical_fuses, bytes([1] * 8))
        other = parse(frame("MACH111\nDESIGN*QF4*F0*E01111111*C00EF*"))
        self.assertFalse(compare(obj, other)["equal"])
        self.assertTrue(compare(obj, other)["fuses_equal"])

    def test_mach_device_identity_is_preserved(self):
        a = parse(frame("MACH111\nA*QF4*F0*"))
        b = parse(frame("MACH110\nA*QF4*F0*"))
        self.assertFalse(compare(a, b)["equal"])

    def test_security_and_unknown_records_cannot_differ_silently(self):
        for left, right in [("G0", "G1"), ("X 123", "X 124"), ("V1 0X", "V1 1X")]:
            a = parse(frame(f"QF8*F0*C0000*{left}*"))
            b = parse(frame(f"QF8*F0*C0000*{right}*"))
            result = compare(a, b)
            self.assertTrue(result["fuses_equal"])
            self.assertFalse(result["equal"])

    def test_transmission_corruption_is_rejected(self):
        good = frame("QF8*F0*C0000*N original*")
        with self.assertRaisesRegex(JedecError, "transmission checksum"):
            parse(good.replace(b"original", b"changed!"))

    def test_fuse_corruption_is_rejected_with_valid_transmission(self):
        with self.assertRaisesRegex(JedecError, "fuse checksum"):
            parse(frame("QF8*F0*L0 1*C0000*"))

    def test_disabled_checksum_and_missing_fuse_checksum_reported(self):
        obj = parse(frame("QF8*F0*", checksum=False) + b"\r\n\x1a")
        self.assertFalse(obj.transmission_verified)
        self.assertIsNone(obj.declared_fuse_checksum)

    def test_malformed_records_are_rejected(self):
        bodies = [
            "QF8*F0*L7 11*",  # Out of range.
            "QF8*F0*L0 0*L0 1*",  # Contradictory explicit data.
            "QF8*L0 1*",  # Missing default, incomplete explicit data.
            "QF8*QF8*F0*",  # Duplicate count.
            "QF8*F0*L0 012*",  # Invalid fuse value.
            "QF8*F0*CXYZW*",  # Invalid checksum.
            "QF8*F0",  # Missing terminator.
            "QF0*F0*",  # Empty fuse map.
        ]
        for body in bodies:
            with self.subTest(body=body), self.assertRaises(JedecError):
                parse(frame(body))

    def test_framing_and_tail_are_rejected(self):
        for data in [b"QF8*F0*", frame("QF8*F0*") + b"garbage",
                     b"\x03\x02QF8*F0*0000", frame("QF8*F0*")[:-1]]:
            with self.subTest(data=data), self.assertRaises(JedecError):
                parse(data)

    def test_equal_repeated_bits_and_multiline_data(self):
        obj = parse(frame("QF8*L0 1000\r\n0001*L0 1*C0081*"))
        self.assertEqual(obj.fuse_checksum, 0x81)

    def test_different_counts_are_not_equal(self):
        result = compare(parse(frame("QF8*F0*")), parse(frame("QF9*F0*")))
        self.assertFalse(result["equal"])
        self.assertEqual(result["different_fuses"], 1)


if __name__ == "__main__":
    unittest.main()
