#!/usr/bin/env python3
"""Original PAIR/P_N fitter policy and dedicated input storage regressions."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from compare_pla import read_pla, compare_pla
from jedec import read, compare


def main():
    root = Path(__file__).with_name("fitter-input-auto-reference")
    cases = json.loads((root / "manifest.json").read_text())["cases"]
    accepted = 0
    with tempfile.TemporaryDirectory() as work:
        for case in cases:
            name = case["name"]
            for extension, digest in case["sha256"].items():
                path = root / (name + "." + extension)
                assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, path
            for compatibility in (False, True):
                command = [sys.argv[1]] + (["--compatibility"] if compatibility else [])
                command += ["--settings", str(root / (name + ".MXL"))]
                source = str(root / (name + ".PDS"))
                pla = subprocess.run(command + ["--emit-pla", source], capture_output=True, timeout=30)
                assert pla.returncode == 0, (name, compatibility, pla.stderr)
                native_pla = Path(work) / "native.pla"
                native_pla.write_bytes(pla.stdout)
                assert compare_pla(read_pla(native_pla), read_pla(root / (name + ".PLA")))["matrix_equal"], (name, compatibility)
                fitted = subprocess.run(command + ["--emit-jedec", source], capture_output=True, timeout=30)
                if not case["original_fit_success"]:
                    assert fitted.returncode != 0 and not fitted.stdout, (name, compatibility)
                    continue
                assert fitted.returncode == 0, (name, compatibility, fitted.stderr)
                native_jed = Path(work) / "native.jed"
                native_jed.write_bytes(fitted.stdout)
                result = compare(read(native_jed), read(root / (name + ".JED")))
                assert result["equal"], (name, compatibility, result)
            accepted += case["original_fit_success"]
    print(f"{len(cases)} automatic input-pair cases: {accepted} full fuse maps and {len(cases)-accepted} rejections matched in both modes")


if __name__ == "__main__":
    main()
