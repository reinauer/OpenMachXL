#!/usr/bin/env python3
"""Measure how much of src/ the test suite executes.

Builds an instrumented tree with clang, runs CTest and prints line and branch
coverage per source file. With --diagnostics it also lists the error messages
in src/ that no test produced; each of those is behaviour nothing checks.

    tools/coverage.py --build build-coverage [--diagnostics] [--reuse]
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLAGS = "-fprofile-instr-generate -fcoverage-mapping -O1"
DIAGNOSTIC = re.compile(r"\b(fail|error|unsupported|LogicError)\(")


def llvm(tool):
    """The LLVM tool that matches the compiler: on PATH, or through xcrun on macOS."""
    if shutil.which(tool):
        return [tool]
    if shutil.which("xcrun"):
        return ["xcrun", tool]
    sys.exit(tool + " not found; coverage needs clang and its llvm tools")


def run(command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", type=Path, default=ROOT / "build-coverage", help="instrumented build directory")
    parser.add_argument("--reuse", action="store_true", help="report from profiles of an earlier run")
    parser.add_argument("--diagnostics", action="store_true", help="list error messages no test produced")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    args = parser.parse_args()
    build, profiles = args.build.resolve(), args.build.resolve() / "profiles"
    if not args.reuse:
        run(["cmake", "-S", str(ROOT), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_CXX_FLAGS=" + FLAGS,
             "-DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate"], stdout=subprocess.DEVNULL)
        run(["cmake", "--build", str(build), "--parallel", str(args.jobs)], stdout=subprocess.DEVNULL)
        shutil.rmtree(profiles, ignore_errors=True)
        environment = dict(os.environ, LLVM_PROFILE_FILE=str(profiles / "%p-%m.profraw"))
        tests = subprocess.run(["ctest", "-j", str(args.jobs), "--timeout", "1800"], cwd=build, env=environment,
                               capture_output=True, text=True)
        print(tests.stdout.strip().splitlines()[-3] if tests.returncode == 0 else tests.stdout[-3000:])
        if tests.returncode:
            sys.exit("the suite failed in the instrumented build; coverage would be misleading")
    raw = sorted(profiles.glob("*.profraw"))
    if not raw:
        sys.exit("no profiles in " + str(profiles))
    merged = build / "coverage.profdata"
    # One profile per test process: far too many for a command line.
    listing = build / "coverage-profiles.txt"
    listing.write_text("".join(str(p) + "\n" for p in raw))
    run(llvm("llvm-profdata") + ["merge", "-sparse", "-o", str(merged), "--input-files=" + str(listing)])
    binaries = [build / "machxl"] + sorted(p for p in build.iterdir()
                                             if p.is_file() and os.access(p, os.X_OK)
                                             and (p.name.endswith("_test") or p.name.endswith("_driver")))
    objects = [str(binaries[0])] + [arg for binary in binaries[1:] for arg in ("-object", str(binary))]
    common = objects + ["-instr-profile=" + str(merged)]
    report = run(llvm("llvm-cov") + ["report"] + common, capture_output=True, text=True).stdout
    rows = []
    for line in report.splitlines():
        fields = line.split()
        if len(fields) == 13 and fields[0].startswith("src/"):
            rows.append((fields[0], int(fields[7]), int(fields[8]), int(fields[10]), int(fields[11])))
    rows.sort(key=lambda row: -row[2])
    print("%-38s %7s %7s %8s   %8s %7s %8s" % ("file", "lines", "missed", "covered", "branches", "missed", "covered"))
    for name, lines, lines_missed, branches, branches_missed in rows:
        if lines:
            print("%-38s %7d %7d %7.1f%%   %8d %7d %8s" % (
                name, lines, lines_missed, 100 - 100 * lines_missed / lines, branches, branches_missed,
                "%.1f%%" % (100 - 100 * branches_missed / branches) if branches else "-"))
    lines, lines_missed = sum(r[1] for r in rows), sum(r[2] for r in rows)
    branches, branches_missed = sum(r[3] for r in rows), sum(r[4] for r in rows)
    print("%-38s %7d %7d %7.1f%%   %8d %7d %7.1f%%" % ("src total", lines, lines_missed,
                                                       100 - 100 * lines_missed / lines, branches, branches_missed,
                                                       100 - 100 * branches_missed / branches))
    if args.diagnostics:
        print("\nError messages no test produced:")
        for name, *_ in sorted(rows):
            shown = run(llvm("llvm-cov") + ["show"] + common + [str(ROOT / name)], capture_output=True, text=True).stdout
            for line in shown.splitlines():
                parts = line.split("|", 2)
                if len(parts) == 3 and parts[1].strip() == "0" and DIAGNOSTIC.search(parts[2]):
                    print("  %s:%s: %s" % (name, parts[0].strip(), parts[2].strip()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
