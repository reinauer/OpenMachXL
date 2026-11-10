#!/usr/bin/env python3
"""Verify Blizzard frontend outputs against saved original DOS references.

Requires only the native compiler and repository fixtures. Both normal and
compatibility modes must match the original labeled PLA matrices and Boolean
functions. All seven designs must also match every main/electrical fuse
and auxiliary record using the native fitter.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from compare_pla import read_pla
from frontend_corpus import compare
import jedec


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def prepare_plcc(source):
    """Reproduce the recorded external wrapper's package-pin conversion."""
    text = source.decode("latin-1").replace("\r\n", "\n").replace("\r", "\n")

    def replace_pin(match):
        number = str((int(match[2]) - 1 + 6) % 44 + 1)
        width = len(match[2]) + len(match[3])
        return match[1] + number + " " * max(1, width - len(number))

    text = re.sub(r"(?im)^(\s*PIN\s+)(\d+)(\s+)", replace_pin, text)
    return text.replace("\n", "\r\n").encode("latin-1")


def main():
    compiler = Path(sys.argv[1]).resolve()
    compiler_digest = digest(compiler.read_bytes())
    fixtures = ROOT / "tests/blizzard-corpus"
    manifest = json.loads((fixtures / "manifest.json").read_text())
    names = [case["name"] for case in manifest["cases"]]
    require(len(names) == len(set(names)) == 7 and
            set(names) == {"U1", "U7", "U8", "U9", "U10", "U11", "U14"},
            "Blizzard manifest must contain all seven designs exactly once")
    with tempfile.TemporaryDirectory() as temporary:
        for case in manifest["cases"]:
            name = case["name"]
            folder = fixtures / name
            for filename, expected in case["files"].items():
                require(Path(filename).name == filename, "Unsafe fixture filename")
                data = (folder / filename).read_bytes()
                require(len(data) == expected["bytes"] and
                        digest(data) == expected["sha256"],
                        "Fixture integrity mismatch: " + filename)
            prepared = folder / (name + ".PDS")
            require(prepare_plcc((folder / (name + ".tqfp.pds")).read_bytes()) ==
                    prepared.read_bytes(), "Package conversion mismatch: " + name)
            original_jedec = jedec.read(folder / (name + ".JED"))
            expected = case["original_jedec"]
            require(original_jedec.transmission_verified and
                    original_jedec.declared_fuse_checksum is not None,
                    "Original JEDEC must contain valid checksums: " + name)
            require((len(original_jedec.fuses), len(original_jedec.electrical_fuses),
                     original_jedec.fuse_checksum, original_jedec.transmission_checksum) ==
                    (expected["fuses"], expected["electrical_fuses"],
                     expected["fuse_checksum"], expected["transmission_checksum"]),
                    "Original JEDEC metadata mismatch: " + name)
            original = read_pla(folder / (name + ".PLA"))
            for mode, flags in [("default", []), ("compatibility", ["--compatibility"])]:
                command = [str(compiler), *flags, "--emit-pla", str(prepared),
                           "--settings", str(folder / (name + ".MXL"))]
                process = subprocess.run(command, capture_output=True, timeout=60)
                require(process.returncode == 0,
                        (name, mode, process.stderr.decode(errors="replace")))
                output = Path(temporary) / (name + "." + mode + ".PLA")
                output.write_bytes(process.stdout)
                result = compare(read_pla(output), original)
                require(result["matrix_identical"] and result["equivalent"],
                        (name, mode, result))
                if name in {"U1", "U7", "U8", "U9", "U10", "U11", "U14"}:
                    command[command.index("--emit-pla")] = "--emit-jedec"
                    process = subprocess.run(command, capture_output=True, timeout=60)
                    require(process.returncode == 0,
                            (name, mode, process.stderr.decode(errors="replace")))
                    image = jedec.parse(process.stdout)
                    result = jedec.compare(image, original_jedec)
                    require(result["equal"] and image.transmission_verified,
                            (name, mode, "native JEDEC", result))
            print(name + ": exact PLA in both modes; original JEDEC checksums valid")
            if name in {"U1", "U7", "U8", "U9", "U10", "U11", "U14"}:
                print(name + ": native main/electrical fuses and auxiliary records exact in both modes")
    require(digest(compiler.read_bytes()) == compiler_digest,
            "Native compiler changed during Blizzard comparison")
    return 0


if __name__ == "__main__":
    sys.exit(main())
