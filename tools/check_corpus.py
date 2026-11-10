#!/usr/bin/env python3
"""Report native/original frontend coverage without treating partial success as completion."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

from compare_pla import PlaError, compare_pla, read_pla


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--settings", action="store_true", help="Pass the reference MXL via --settings")
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--require-complete", action="store_true")
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads((args.reference / "manifest.json").read_text())
    report = {"native_executable_sha256": hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
              "scope": "Boolean and ordered PLA cover comparison; not placement/routing/JEDEC verification",
              "settings_applied": args.settings, "cases": []}
    for case in manifest["results"]:
        name = Path(case["example"]).stem
        original = args.reference / name.lower() / "WORK"
        native = args.output / (name + ".PLA")
        command = [str(args.compiler), "--compatibility", "--emit-pla", str(original / (name + ".PDS"))]
        if args.settings:
            command += ["--settings", str(original / (name + ".MXL"))]
        result = {"name": name, "original_fit_succeeded": case["success"],
                  "native_fuse_comparison": "not performed", "command": command}
        start = time.monotonic()
        try:
            process = subprocess.run(command, capture_output=True, timeout=args.timeout)
            result["returncode"] = process.returncode
            result["stderr"] = process.stderr.decode("utf-8", errors="replace")
            if process.returncode == 0:
                native.write_bytes(process.stdout)
                try:
                    result["pla"] = compare_pla(read_pla(original / (name + ".PLA")), read_pla(native),
                                               max_nodes=250000, max_work=5000000)
                except (PlaError, RecursionError) as error:
                    result["comparison_error"] = str(error)
            elif process.stdout:
                result["partial_output_on_failure"] = True
        except subprocess.TimeoutExpired:
            result["timed_out"] = True
        result["seconds"] = round(time.monotonic() - start, 3)
        report["cases"].append(result)
        (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        status = ("equivalent" if result.get("pla", {}).get("equivalent") else
                  "different" if "pla" in result else "unverified")
        print(name + ": " + status, flush=True)
    complete = all(case.get("pla", {}).get("equivalent") for case in report["cases"])
    report["all_frontend_functions_equivalent"] = complete
    report["complete_compiler_verified"] = False
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return 1 if args.require_complete and not complete else 0


if __name__ == "__main__":
    raise SystemExit(main())
