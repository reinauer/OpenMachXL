#!/usr/bin/env python3
"""Require original ordered PLAs and JEDEC configurations for the full archive.

This development tool runs only the native compiler. Original outputs are
hash-checked fixtures. Boolean equivalence, ordered matrices, fuse maps and
file bytes are reported separately; finite corpus coverage is not proof of
complete language support. A mismatch exits 1 unless --report-only is used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from frontend_corpus import compare as compare_functions
from compare_pla import parse_pla
import jedec


def digest(data):
    return hashlib.sha256(data).hexdigest()


def checked(data, expected, description):
    if digest(data) != expected:
        raise ValueError("Reference hash mismatch: " + description)
    return data


def error_codes(text):
    # The original can warn about reduced fit quality before its final error.
    # Reproducing a fit failure does not require calling its warnings errors.
    lines = (line for line in text.splitlines() if not re.search(r"\b(?:WARNING|INFO)\b", line, re.I))
    return sorted(set(re.findall(r"\bz\d+\b", "\n".join(lines))))


def validate_case_set(cases, archive_names):
    expected = {Path(name).stem for name in archive_names if name.upper().endswith(".PDS")}
    actual = [case["name"] for case in cases]
    if len(set(actual)) != len(actual) or set(actual) != expected:
        raise ValueError("Reference manifest must cover every archived PDS exactly once; "
                         "missing=" + repr(sorted(expected - set(actual))) +
                         ", extra=" + repr(sorted(set(actual) - expected)))


def invoke(command, destination, timeout):
    try:
        result = subprocess.run(command, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        return {"command": command, "timed_out": True,
                "partial_output_on_failure": bool(error.stdout)}, None
    outcome = {"command": command, "returncode": result.returncode,
               "stderr": result.stderr.decode("utf-8", errors="replace"),
               "partial_output_on_failure": bool(result.returncode and result.stdout)}
    if result.returncode:
        return outcome, None
    destination.write_bytes(result.stdout)
    outcome["sha256"] = digest(result.stdout)
    return outcome, result.stdout


def case_matches(case):
    """Do not count arbitrary rejection as reproducing an original failure."""
    pla = case.get("pla", {})
    if not pla.get("comparison", {}).get("matrix_identical"):
        return False
    fit = case.get("jedec", {})
    if case["original_fit_succeeded"]:
        return fit.get("comparison", {}).get("equal", False)
    expected = case.get("original_error_codes", [])
    actual = error_codes(fit.get("stderr", ""))
    return (bool(expected) and actual == expected and fit.get("returncode", 0) > 0
            and not fit.get("partial_output_on_failure", False))


def summarize(cases):
    return {
        "cases": len(cases),
        "boolean_matches": sum(c.get("pla", {}).get("comparison", {}).get("equivalent", False) for c in cases),
        "ordered_matrix_matches": sum(c.get("pla", {}).get("comparison", {}).get("matrix_identical", False) for c in cases),
        "original_successful_fits": sum(c["original_fit_succeeded"] for c in cases),
        "native_successful_fits": sum(c.get("jedec", {}).get("returncode") == 0 for c in cases),
        "fuse_configuration_matches": sum(c.get("jedec", {}).get("comparison", {}).get("equal", False) for c in cases),
        "exact_jedec_file_matches": sum(c.get("jedec", {}).get("comparison", {}).get("bytes_equal", False) for c in cases),
        "archive_cases_matching": sum(case_matches(c) for c in cases),
        "all_archive_results_match": bool(cases) and all(case_matches(c) for c in cases),
        # Passing every bundled example cannot prove all accepted PDS programs.
        "complete_compiler_verified": False,
    }


def run(compiler, reference, output, timeout):
    manifest = json.loads((reference / "manifest.json").read_text())
    compiler_hash = digest(compiler.read_bytes())
    for archive in ("EXAMPLES.ZIP", "DAT.ZIP"):
        checked((ROOT / "dos" / archive).read_bytes(), manifest["archives"][archive], archive)
    output.mkdir(parents=True, exist_ok=False)
    report = {"scope": __doc__, "native_executable_sha256": compiler_hash,
              "dos_compatibility": True,
              "manifest_sha256": digest((reference / "manifest.json").read_bytes()),
              "reference_archives": manifest["archives"], "cases": []}
    with ZipFile(ROOT / "dos/EXAMPLES.ZIP") as examples, ZipFile(ROOT / "dos/DAT.ZIP") as dat:
        validate_case_set(manifest["cases"], examples.namelist())
        archives = {"EXAMPLES.ZIP": examples, "DAT.ZIP": dat}
        for original in manifest["cases"]:
            name = original["name"]
            directory = output / name
            directory.mkdir()
            source = directory / (name + ".PDS")
            source.write_bytes(checked(examples.read(name + ".PDS"), original["source_sha256"], source.name))
            settings_archive, member = original["settings_source"].split("/", 1)
            settings = directory / (name + ".MXL")
            settings.write_bytes(checked(archives[settings_archive].read(member),
                                        original["original_settings_sha256"], settings.name))
            fixtures = {}
            for filename, identity in original["files"].items():
                fixtures[Path(filename).suffix] = checked((reference / name / filename).read_bytes(),
                                                         identity["sha256"], name + "/" + filename)
            case = {"name": name, "original_fit_succeeded": original["success"],
                    "original_error_codes": error_codes(fixtures.get(".LOG", b"").decode("latin1"))}
            for stage, option, suffix in (("pla", "--emit-pla", ".PLA"), ("jedec", "--emit-jedec", ".JED")):
                command = [str(compiler), "--compatibility", option, str(source), "--settings", str(settings)]
                outcome, native = invoke(command, directory / (name + suffix), timeout)
                if native is not None:
                    try:
                        if stage == "pla":
                            comparison = compare_functions(parse_pla(native), parse_pla(fixtures[suffix]))
                            comparison["bytes_equal"] = native == fixtures[suffix]
                            outcome["comparison"] = comparison
                        elif suffix in fixtures:
                            outcome["comparison"] = jedec.compare(jedec.parse(fixtures[suffix]), jedec.parse(native))
                        else:
                            outcome["unexpected_success"] = True
                    except (ValueError, AssertionError, RecursionError) as error:
                        outcome["comparison_error"] = str(error)
                case[stage] = outcome
            report["cases"].append(case)
            report["summary"] = summarize(report["cases"])
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
            print(name + ": " + ("matches" if case_matches(case) else "INCOMPLETE"), flush=True)
    if digest(compiler.read_bytes()) != compiler_hash:
        report["invalidated"] = "Native executable changed during verification"
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        raise ValueError(report["invalidated"])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("--reference", type=Path, default=ROOT / "tests/corpus")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--report-only", action="store_true", help="Write results without failing on compatibility gaps")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    try:
        report = run(args.compiler.resolve(), args.reference.resolve(), args.output.resolve(), args.timeout)
    except (OSError, ValueError, KeyError) as error:
        print("verify_compatibility: " + str(error), file=sys.stderr)
        return 2
    print(json.dumps(report["summary"], indent=2))
    return 0 if args.report_only or report["summary"]["all_archive_results_match"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
