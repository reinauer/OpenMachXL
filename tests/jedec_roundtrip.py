#!/usr/bin/env python3
"""Check native serializer against independently parsed original JEDEC files."""
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import jedec


def main():
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        for source in sorted((ROOT / "tests/reference").glob("*.JED")):
            original = jedec.read(source)
            (directory / "fuses").write_bytes(original.fuses)
            (directory / "electrical").write_bytes(original.electrical_fuses)
            pin_record = next(record for record in original.auxiliary_records if record.startswith("QP"))
            pin_count = int(pin_record[2:])
            completed = subprocess.run([str(executable), str(directory / "fuses"),
                                        str(directory / "electrical"), original.device, str(pin_count)],
                                       capture_output=True, check=True)
            encoded = jedec.parse(completed.stdout)
            comparison = jedec.compare(original, encoded)
            if not comparison["equal"]:
                raise AssertionError(json.dumps(comparison))
            if not encoded.transmission_verified or encoded.declared_fuse_checksum is None:
                raise AssertionError("Checksum omitted")
            if original.device == "MACH111":
                marker = b"NOTE  Central Switch Matrix fuses for BLOCK A *"
                def physical_records(data):
                    return data[data.index(marker):data.index(b"\x03")]
                if physical_records(source.read_bytes()) != physical_records(completed.stdout):
                    raise AssertionError("Original MACH111 physical records differ: " + source.name)
            print(source.name + ": fuse/configuration roundtrip passed")


if __name__ == "__main__":
    main()
