#!/usr/bin/env python3
"""Compare all bundled native frontend equations with original compiler PLAs.

Original outputs are separate regression fixtures, never production compiler input.
It compares semantic functions after decoding original hardware-XOR splitting,
polarity labels, and the two observed long-name aliases in DATA_AQ. It does not
prove identical PLA ordering, physical pairing, fitting, or fuse maps.
"""
import argparse
from dataclasses import replace
import json
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from compare_pla import Bdd, read_pla

# These are symbol aliases observed in the original reference, never Boolean
# function data. Production frontend code does not import this test module.
ALIASES = {"RAM_AD001": "RAM_ADDRESS", "MUXED_002": "MUXED_DATA"}


def alias(label):
    for short, full in ALIASES.items():
        if label == short or label.startswith(short + "["):
            return full + label[len(short):]
    return label


def normalize(pla):
    return replace(pla, inputs=tuple(alias(n) for n in pla.inputs),
                   outputs=tuple(alias(n.rstrip("-%")) for n in pla.outputs),
                   phase="".join(str(int(p) ^ (0 if ".XOR" in n else ((len(n) - len(n.rstrip("-%"))) % 2)))
                                 for n, p in zip(pla.outputs, pla.phase)))


def roots(bdd, pla):
    functions = bdd.compile(normalize(pla))
    phases={alias(n.rstrip("-%")):n[len(n.rstrip("-%")):] for n in pla.outputs}
    for name in list(functions):
        if ".XORL" not in name:
            continue
        right = name.replace(".XORL", ".XORR")
        if right not in functions:
            raise AssertionError("Unpaired hardware XOR output: " + name)
        left_root, right_root = functions.pop(name), functions.pop(right)
        both = bdd.complement(bdd.either(bdd.complement(left_root),
                                        bdd.complement(right_root)))
        either = bdd.either(left_root, right_root)
        xor = bdd.complement(bdd.either(bdd.complement(either), both))
        out = name.replace(".XORL_REG", ".REG").replace(".XORL_T", ".T").replace(".XORL_G", ".G").replace(".XORL", "")
        # Constant legs omit their redundant common phase marker. The
        # nonconstant partner still carries the output phase.
        phase=phases[name]
        if phases[name]!=phases[right]:
            if left_root in (0,1) and not phases[name]:phase=phases[right]
            elif right_root in (0,1) and not phases[right]:phase=phases[name]
            else:raise AssertionError("Mismatched common XOR output polarity")
        if len(phase) % 2:
            xor=bdd.complement(xor)
        if out in functions:
            raise AssertionError("Duplicate XOR output: " + out)
        functions[out] = xor
    return functions


def compare(native, original):
    variables = sorted(set(normalize(native).inputs) | set(normalize(original).inputs))
    bdd = Bdd(variables, 1_000_000, 20_000_000)
    lhs, rhs = roots(bdd, native), roots(bdd, original)
    shared = lhs.keys() & rhs.keys()
    different = sorted(n for n in shared if lhs[n] != rhs[n])
    # Record strict, unnormalized identity separately: a successful semantic
    # comparison does not hide source symbol, XOR selection or row differences.
    return {
        "raw_input_labels_identical": native.inputs == original.inputs,
        "raw_output_labels_identical": native.outputs == original.outputs,
        "equivalent": not different and lhs.keys() == rhs.keys(),
        "functions": len(shared), "different": different,
        "native_only": sorted(lhs.keys() - rhs.keys()),
        "original_only": sorted(rhs.keys() - lhs.keys()),
        "matrix_identical": (native.inputs, native.outputs, native.phase, native.rows)
                            == (original.inputs, original.outputs, original.phase, original.rows),
        "rows": [len(native.rows), len(original.rows)],
        "counterexamples": {n: bdd.witness(lhs[n], rhs[n]) for n in different[:3]},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("--reference", type=Path, default=ROOT / "tests/corpus",
                        help="Hash-verified saved original compiler fixtures")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    compiler_digest=hashlib.sha256(args.compiler.resolve().read_bytes()).hexdigest()
    results = {}
    expectations=json.loads((ROOT / "tests/frontend-expectations.json").read_text())
    # The list must name every corpus design, so a new design is added on purpose.
    required_matrices=set(expectations["required_exact_matrices"])
    manifest=json.loads((args.reference / "manifest.json").read_text())
    cases={case["name"]:case for case in manifest["cases"]}
    archive_path=ROOT / "dos/EXAMPLES.ZIP"
    if hashlib.sha256(archive_path.read_bytes()).hexdigest()!=manifest["archives"]["EXAMPLES.ZIP"]:
        raise AssertionError("Original source archive hash mismatch")
    with ZipFile(ROOT / "dos/EXAMPLES.ZIP") as archive, ZipFile(ROOT / "dos/DAT.ZIP") as dat, tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for name in sorted(n for n in archive.namelist() if n.upper().endswith(".PDS")):
            source = tmp / name
            if source.stem not in required_matrices:
                raise AssertionError("Corpus design missing from required_exact_matrices: "+source.stem)
            data=archive.read(name)
            case=cases[source.stem]
            if hashlib.sha256(data).hexdigest()!=case["source_sha256"]:
                raise AssertionError("Source hash mismatch: "+name)
            source.write_bytes(data)
            settings_archive,settings_name=case["settings_source"].split("/",1)
            settings_data=(archive if settings_archive=="EXAMPLES.ZIP" else dat).read(settings_name)
            if hashlib.sha256(settings_data).hexdigest()!=case["original_settings_sha256"]:
                raise AssertionError("Original settings hash mismatch: "+name)
            settings_path=source.with_suffix(".MXL");settings_path.write_bytes(settings_data)
            proc = subprocess.run([str(args.compiler.resolve()), "--compatibility", "--emit-pla", str(source),
                                   "--settings",str(settings_path)],
                                  capture_output=True)
            if proc.returncode:
                results[source.stem] = {"equivalent": False, "error": proc.stderr.decode()}
            else:
                target = source.with_suffix(".PLA")
                target.write_bytes(proc.stdout)
                original = args.reference / source.stem / target.name.upper()
                if hashlib.sha256(original.read_bytes()).hexdigest()!=case["files"][target.name.upper()]["sha256"]:
                    raise AssertionError("Original PLA hash mismatch: "+name)
                results[source.stem] = compare(read_pla(target), read_pla(original))
            result=results[source.stem]
            # Every corpus design matches the original matrix; a design that
            # is missing from the expectations must not be held to less.
            result["required_matrix_regression"]=not result.get("matrix_identical",False)
            status="equivalent" if result["equivalent"] else "DIFFERENT"
            if result["required_matrix_regression"]:status+="; REQUIRED MATRIX REGRESSED"
            print(source.stem + ": " + status, flush=True)
    if hashlib.sha256(args.compiler.resolve().read_bytes()).hexdigest()!=compiler_digest:
        raise AssertionError("Native compiler changed during corpus comparison; rerun with one stable binary")
    if args.report:
        args.report.write_text(json.dumps({"scope": __doc__, "dos_compatibility":True, "compiler_sha256":compiler_digest,
                                          "source_archive_sha256":manifest["archives"]["EXAMPLES.ZIP"],
                                          "results": results}, indent=2) + "\n")
    return 0 if all(r["equivalent"] and not r["required_matrix_regression"] for r in results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
