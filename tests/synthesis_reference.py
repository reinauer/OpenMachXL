#!/usr/bin/env python3
"""Compare native synthesis options against original ordered PLA matrices."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from compare_pla import compare_pla, read_pla


def main():
    executable = str(Path(sys.argv[1]).resolve())
    fixtures = ROOT / "tests/synthesis_reference"
    manifest = json.loads((fixtures / "manifest.json").read_text())
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        for case in manifest["cases"]:
            name = case["name"]
            for filename, expected in case["files"].items():
                if hashlib.sha256((fixtures / filename).read_bytes()).hexdigest() != expected:
                    raise AssertionError("Fixture changed: " + filename)
            result = subprocess.run([executable, "--compatibility", "--emit-pla", str(fixtures / (name + ".PDS")),
                                     "--settings", str(fixtures / (name + ".MXL"))],
                                    capture_output=True, check=True, timeout=30)
            native = directory / (name + ".PLA")
            native.write_bytes(result.stdout)
            comparison = compare_pla(read_pla(native), read_pla(fixtures / (name + ".PLA")))
            if not comparison["equivalent"] or not comparison["matrix_equal"]:
                raise AssertionError(name + ": " + json.dumps(comparison))
            print(name + ": exact labeled, ordered PLA matrix")
        for option in ("[GATE_SPLIT]=Y\n[THRESHOLD_12]=0", "[M_POLAR]=not-a-policy", "[FAST_MIN]=invalid"):
            settings = directory / "bad.mxl"
            settings.write_text(option + "\n")
            result = subprocess.run([executable, "--emit-pla", str(fixtures / "BASE.PDS"),
                                     "--settings", str(settings)], capture_output=True, timeout=30)
            if result.returncode == 0 or result.stdout or not result.stderr:
                raise AssertionError("Invalid/unimplemented option accepted: " + option)


if __name__ == "__main__":
    main()
