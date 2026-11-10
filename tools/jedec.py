#!/usr/bin/env python3
"""Validate and compare ASCII JEDEC fuse maps without third-party packages.

Supports QF, F, L, binary E and C records. Notes and the initial free-text
header do not affect comparison, except its MACH device identifier. All
other records (including security and vectors)
must match in order after whitespace normalization; their device semantics
are not interpreted. This is file comparison, not hardware equivalence.

Checksum conventions are independently described by GALasm's jedec.c:
https://github.com/dwery/galasm/blob/master/src/jedec.c
"""

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import sys


class JedecError(ValueError):
    """Malformed or inconsistent JEDEC data."""


@dataclass(frozen=True)
class Jedec:
    fuses: bytes
    electrical_fuses: bytes
    device: str | None
    auxiliary_records: tuple[str, ...]
    fuse_checksum: int
    declared_fuse_checksum: int | None
    transmission_checksum: int
    transmission_verified: bool
    sha256: str


def fuse_checksum(fuses: bytes) -> int:
    """Sum packed bytes, with lowest numbered fuse in bit zero."""
    return sum(bit << (index % 8) for index, bit in enumerate(fuses)) & 0xffff


def parse(data: bytes, *, max_fuses: int = 16_000_000) -> Jedec:
    """Read one framed JEDEC transmission, validating every present checksum.

    A transmission checksum of 0000 disables that check per JEDEC. Missing
    fuse checksum C is accepted but reported as absent. Without F, every fuse
    must be supplied explicitly; no default is invented. Bytes outside the
    frame may contain a textual preamble and trailing DOS EOF markers.
    """
    if data.count(b"\x02") != 1 or data.count(b"\x03") != 1:
        raise JedecError("expected exactly one STX and one ETX")
    start, end = data.index(b"\x02"), data.index(b"\x03")
    if end <= start:
        raise JedecError("ETX precedes STX")
    tail = re.fullmatch(rb"([0-9A-Fa-f]{4})[ \t\r\n\x1a]*", data[end + 1:])
    if tail is None:
        raise JedecError("expected four hex checksum digits immediately after ETX")
    declared_transmission = int(tail[1], 16)
    actual_transmission = sum(data[start:end + 1]) & 0xffff
    if declared_transmission and declared_transmission != actual_transmission:
        raise JedecError(
            f"transmission checksum {declared_transmission:04X} "
            f"does not match {actual_transmission:04X}"
        )
    try:
        body = data[start + 1:end].decode("ascii")
    except UnicodeDecodeError as exc:
        raise JedecError("non-ASCII data in JEDEC frame") from exc
    if any(ord(c) < 32 and c not in "\r\n\t" for c in body):
        raise JedecError("unexpected control character in JEDEC frame")
    parts = body.split("*")
    if parts[-1].strip():
        raise JedecError("record missing terminating asterisk")
    count = default = declared_fuses = None
    electrical = None
    device = None
    links = []
    auxiliary = []
    for index, part in enumerate(parts[:-1]):
        record = part.strip()
        if not record:
            continue
        # The first field may be a human-readable design specification.
        # Recognizable command prefixes are never discarded as a header.
        if index == 0 and not re.match(r"(?:Q[FPV]\s*\d|[A-Z](?:\s|\d)|N)", record):
            device_match = re.match(r"(MACH\d+)\b", record)
            if device_match:
                device = device_match[1]
            continue
        if record.startswith("N"):
            continue
        if record.startswith("QF"):
            match = re.fullmatch(r"QF\s*(\d+)", record)
            if not match or count is not None:
                raise JedecError("invalid or duplicate QF record")
            count = int(match[1])
            if not 0 < count <= max_fuses:
                raise JedecError(f"QF must be between 1 and {max_fuses}")
        elif record.startswith("F"):
            match = re.fullmatch(r"F\s*([01])", record)
            if not match or default is not None:
                raise JedecError("invalid or duplicate F record")
            default = int(match[1])
        elif record.startswith("L"):
            match = re.fullmatch(r"L\s*(\d+)\s+([01\s]+)", record)
            if not match:
                raise JedecError("invalid L record")
            bits = re.sub(r"\s", "", match[2])
            if not bits:
                raise JedecError("empty L record")
            links.append((int(match[1]), bytes(int(c) for c in bits)))
        elif record.startswith("E"):
            match = re.fullmatch(r"E\s*([01\s]+)", record)
            if not match or electrical is not None:
                raise JedecError("invalid or duplicate E record (only binary supported)")
            electrical = bytes(int(c) for c in re.sub(r"\s", "", match[1]))
            if not electrical or len(electrical) > max_fuses:
                raise JedecError("empty or oversized E record")
        elif record.startswith("C"):
            match = re.fullmatch(r"C\s*([0-9A-Fa-f]{4})", record)
            if not match or declared_fuses is not None:
                raise JedecError("invalid or duplicate C record")
            declared_fuses = int(match[1], 16)
        else:
            # Do not silently drop security, user code, vector or unknown data.
            auxiliary.append(" ".join(record.split()))
    if count is None:
        raise JedecError("missing QF record")
    fuses = bytearray([default if default is not None else 0]) * count
    written = bytearray(count)
    for address, bits in links:
        if address + len(bits) > count:
            raise JedecError(f"L record at {address} extends beyond QF{count}")
        for offset, bit in enumerate(bits, address):
            if written[offset] and fuses[offset] != bit:
                raise JedecError(f"contradictory L records at fuse {offset}")
            fuses[offset] = bit
            written[offset] = 1
    if default is None and not all(written):
        raise JedecError("unspecified fuses without an F default")
    # Include E power-down fuses immediately after the QF bits in the
    # checksum stream, including when QF is not byte aligned.
    electrical = electrical or b""
    actual_fuses = fuse_checksum(fuses + electrical)
    if declared_fuses is not None and actual_fuses != declared_fuses:
        raise JedecError(
            f"fuse checksum {declared_fuses:04X} does not match {actual_fuses:04X}"
        )
    return Jedec(bytes(fuses), electrical, device, tuple(auxiliary), actual_fuses, declared_fuses,
                 actual_transmission, declared_transmission != 0,
                 hashlib.sha256(data).hexdigest())


def read(path: Path | str) -> Jedec:
    return parse(Path(path).read_bytes())


def compare(left: Jedec, right: Jedec) -> dict:
    """Compare represented fuses and uninterpreted auxiliary records."""
    mismatch_count = 0
    first_mismatches = []
    for index, (a, b) in enumerate(zip(left.fuses, right.fuses)):
        if a != b:
            mismatch_count += 1
            if len(first_mismatches) < 32:
                first_mismatches.append(index)
    size_difference = abs(len(left.fuses) - len(right.fuses))
    extra_start = min(len(left.fuses), len(right.fuses))
    first_mismatches.extend(range(extra_start, extra_start + min(size_difference, 32 - len(first_mismatches))))
    fuse_equal = not mismatch_count and not size_difference
    electrical_equal = left.electrical_fuses == right.electrical_fuses
    device_equal = left.device == right.device
    auxiliary_equal = left.auxiliary_records == right.auxiliary_records
    return {
        "equal": fuse_equal and electrical_equal and device_equal and auxiliary_equal,
        "bytes_equal": left.sha256 == right.sha256,
        "fuses_equal": fuse_equal,
        "electrical_fuses_equal": electrical_equal,
        "electrical_fuse_counts": [len(left.electrical_fuses), len(right.electrical_fuses)],
        "devices": [left.device, right.device],
        "devices_equal": device_equal,
        "auxiliary_equal": auxiliary_equal,
        "fuse_counts": [len(left.fuses), len(right.fuses)],
        "different_fuses": mismatch_count + size_difference,
        "first_different_fuses": first_mismatches,
        "transmission_verified": [left.transmission_verified, right.transmission_verified],
        "fuse_checksum_present": [left.declared_fuse_checksum is not None,
                                  right.declared_fuse_checksum is not None],
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    parser.add_argument("--json", action="store_true", help="emit machine-readable result")
    parser.add_argument("--exact", action="store_true", help="also require identical file bytes")
    args = parser.parse_args(argv)
    try:
        result = compare(read(args.left), read(args.right))
    except (OSError, JedecError) as exc:
        print(f"jedec: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print("Fuse maps:", "equal" if result["fuses_equal"] else "different")
        print("Electrical fuses:", "equal" if result["electrical_fuses_equal"] else "different")
        print("MACH device:", "equal" if result["devices_equal"] else "different")
        print("Auxiliary records:", "equal" if result["auxiliary_equal"] else "different")
        print("File bytes:", "equal" if result["bytes_equal"] else "different")
        if result["different_fuses"]:
            print("Different fuse positions:", result["different_fuses"])
            print("First differences:", result["first_different_fuses"])
        if not all(result["transmission_verified"]):
            print("At least one transmission checksum is disabled (0000).")
        if not all(result["fuse_checksum_present"]):
            print("At least one file has no C fuse checksum.")
    return 0 if result["equal"] and (not args.exact or result["bytes_equal"]) else 1


if __name__ == "__main__":
    sys.exit(main())
