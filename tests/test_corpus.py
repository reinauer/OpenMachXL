"""Validate provenance and integrity of the saved DOS reference corpus.

These checks do not execute the compiler. They keep the comparison target
stable while compiler behavior changes.
"""
import hashlib
import json
from pathlib import Path
import unittest
from zipfile import ZipFile

from tools.compare_pla import read_pla
from tools.jedec import read as read_jedec

ROOT = Path(__file__).resolve().parents[1]


class OriginalCorpusTests(unittest.TestCase):
    def test_partition_debug_fixture_identity(self):
        corpus = ROOT / "tests/fitter-partition-corpus"
        manifest = json.loads((corpus / "manifest.json").read_text())
        self.assertEqual({case["name"] for case in manifest["cases"]},
                         {"3COUNT_D", "DECODE", "AUTO2", "AUTO3", "AUTO5"})
        for case in manifest["cases"]:
            for filename, expected in case["files"].items():
                path = corpus / case["name"].lower() / filename
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), expected, str(path))

    def test_original_outputs_and_source_identity(self):
        if not (ROOT / "dos/EXAMPLES.ZIP").is_file():
            self.skipTest("DOS archives are not present")
        corpus = ROOT / "tests/corpus"
        manifest = json.loads((corpus / "manifest.json").read_text())
        cases = manifest["cases"]
        self.assertEqual(len(cases), 33)
        self.assertEqual(sum(case["success"] for case in cases), 32)
        self.assertEqual(len({case["name"] for case in cases}), len(cases))
        for name, expected in manifest["archives"].items():
            self.assertEqual(hashlib.sha256((ROOT / "dos" / name).read_bytes()).hexdigest(),
                             expected, name)
        with ZipFile(ROOT / "dos/EXAMPLES.ZIP") as archive:
            for case in cases:
                name = case["name"]
                with self.subTest(name=name):
                    self.assertEqual(hashlib.sha256(archive.read(name + ".PDS")).hexdigest(),
                                     case["source_sha256"])
                    for filename, identity in case["files"].items():
                        data = (corpus / name / filename).read_bytes()
                        self.assertEqual(len(data), identity["bytes"], filename)
                        self.assertEqual(hashlib.sha256(data).hexdigest(), identity["sha256"], filename)
                    pla = read_pla(corpus / name / (name + ".PLA"))
                    self.assertTrue(pla.outputs)
                    jedec = corpus / name / (name + ".JED")
                    if case["success"]:
                        image = read_jedec(jedec)
                        self.assertTrue(image.transmission_verified)
                        self.assertIsNotNone(image.declared_fuse_checksum)
                        self.assertEqual(image.device, pla.device)
                    else:
                        self.assertFalse(jedec.exists())
                        self.assertEqual(name, "NOT2BIG")
                        log = (corpus / name / (name + ".LOG")).read_text()
                        self.assertIn("ERROR z8007", log)
                        self.assertIn("Only 95 percent", log)


if __name__ == "__main__":
    unittest.main()
