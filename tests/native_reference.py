#!/usr/bin/env python3
"""Compare native frontend functions with actual original-compiler outputs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from compare_pla import read_pla, compare_pla


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    reference = ROOT / "tests/reference"
    manifest = json.loads((reference / "manifest.json").read_text())
    with ZipFile(ROOT / "dos/EXAMPLES.ZIP") as archive, tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for case in manifest["cases"]:
            name = case["name"]
            source = archive.read(name + ".PDS")
            original = reference / (name + ".PLA")
            if hashlib.sha256(source).hexdigest() != case["source_sha256"]:
                raise AssertionError(name + " source changed")
            if hashlib.sha256(original.read_bytes()).hexdigest() != case["pla_sha256"]:
                raise AssertionError(name + " reference changed")
            input_path = tmp / (name + ".PDS")
            input_path.write_bytes(source)
            result = subprocess.run([str(compiler), "--compatibility", "--emit-pla", str(input_path)],
                                    capture_output=True, check=True)
            native = tmp / (name + ".PLA")
            native.write_bytes(result.stdout)
            comparison = compare_pla(read_pla(original), read_pla(native))
            if not comparison["equivalent"]:
                raise AssertionError(f"{name}: {comparison}")
            print(name + ": equivalent", flush=True)
        # Invalid designs must fail without emitting a partial PLA.
        for number, source in enumerate([
            b"CHIP BAD MACH111\nPIN ? Y\nEQUATIONS\nY=UNDECLARED\n",
            b"CHIP BAD MACH111\nPIN ? A\nPIN ? Y\nEQUATIONS\nY=(A+\n",
            b"CHIP BAD MACH111\nPIN ? A\nPIN ? Y\nEQUATIONS\nUNKNOWN_SYNTAX A\n",
        ]):
            input_path = tmp / ("INVALID" + str(number) + ".PDS")
            input_path.write_bytes(source)
            result = subprocess.run([str(compiler), "--emit-pla", str(input_path)], capture_output=True)
            if result.returncode == 0 or result.stdout or not result.stderr:
                raise AssertionError("Accepted invalid source")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
