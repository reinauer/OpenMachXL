#!/usr/bin/env python3
"""Exactly compare Boolean functions in MACHXL binary .type f PLA files.

Uses a shared reduced ordered BDD, not sampled input vectors. The emitted
cover is interpreted in each output's .phase (0 complements the cover).
AMD metadata is ignored except an optional DEVICE identity. This does not
compare placement constraints, register types or sequential simulation.

PLA .type f rows specify only the on-set; output 0, -, and ~ add no term.
Reference: https://www.cs.nthu.edu.tw/~cad/cad90/Problems/P5/espresso.5.pdf
"""

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import sys


class PlaError(ValueError):
    """Unsupported, malformed, or budget-exhausting PLA comparison."""


@dataclass(frozen=True)
class Pla:
    inputs: tuple[str, ...]
    outputs: tuple[str, ...]
    phase: str
    rows: tuple[tuple[str, str], ...]
    device: str | None
    sha256: str


def parse_pla(data: bytes | str, *, max_inputs: int = 256,
              max_outputs: int = 4096, max_rows: int = 1_000_000) -> Pla:
    if isinstance(data, str):
        data = data.encode("utf-8")
    # Metadata may contain DOS text; core directives and matrices are ASCII.
    lines = data.decode("latin1").rstrip("\x1a\r\n ").splitlines()
    declarations = {}
    rows = []
    device = None
    ended = False
    pending = None
    pending_tokens = []
    pending_count = 0

    def error(line_number, message):
        raise PlaError(f"line {line_number}: {message}")

    def declare(key, value, line_number):
        if key in declarations:
            error(line_number, f"duplicate {key} directive")
        declarations[key] = value

    for number, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#"):
            match = re.fullmatch(r"#AMD\s+DEVICE\s+(\S+)", line)
            if match:
                if device is not None and device != match[1]:
                    error(number, "conflicting AMD device names")
                device = match[1]
            continue
        if ended:
            error(number, "data after .e")
        if pending:
            if line.startswith("."):
                error(number, f"too few labels for {pending}")
            pending_tokens.extend(line.rstrip("\\").split())
            if len(pending_tokens) > pending_count:
                error(number, f"too many labels for {pending}")
            if len(pending_tokens) == pending_count:
                declare(pending, tuple(pending_tokens), number)
                pending = None
            continue
        if line.startswith("."):
            words = line.split()
            key, args = words[0], words[1:]
            if key in (".e", ".end"):
                if args:
                    error(number, "unexpected arguments after .e")
                ended = True
                continue
            if rows:
                error(number, "directive after product rows")
            if key in (".i", ".o", ".p"):
                if len(args) != 1 or not re.fullmatch(r"\d+", args[0]):
                    error(number, f"invalid {key} count")
                value = int(args[0])
                maximum = {".i": max_inputs, ".o": max_outputs, ".p": max_rows}[key]
                if value > maximum or (key == ".o" and value == 0):
                    error(number, f"{key} count outside supported limit")
                declare(key, value, number)
            elif key == ".type":
                if args != ["f"]:
                    error(number, "only .type f is supported")
                declare(key, "f", number)
            elif key in (".ilb", ".ob"):
                count_key = ".i" if key == ".ilb" else ".o"
                if count_key not in declarations:
                    error(number, f"{count_key} must precede {key}")
                if key in declarations:
                    error(number, f"duplicate {key} directive")
                count = declarations[count_key]
                if args and args[-1] == "\\":
                    args.pop()
                if len(args) > count:
                    error(number, f"too many labels for {key}")
                if len(args) == count:
                    declare(key, tuple(args), number)
                else:
                    pending, pending_tokens, pending_count = key, args, count
            elif key == ".phase":
                if ".o" not in declarations:
                    error(number, ".o must precede .phase")
                phase = "".join(args)
                if len(phase) != declarations[".o"] or set(phase) - set("01"):
                    error(number, "invalid .phase width or symbol")
                declare(key, phase, number)
            else:
                error(number, f"unsupported directive {key}")
            continue
        for required in (".type", ".i", ".o", ".ilb", ".ob", ".p"):
            if required not in declarations:
                error(number, f"missing {required} before product rows")
        parts = line.split()
        if declarations[".i"] == 0 and len(parts) == 1:
            parts.insert(0, "")
        if len(parts) != 2:
            error(number, "expected input and output product columns")
        inputs, outputs = parts
        if len(inputs) != declarations[".i"] or set(inputs) - set("01-"):
            error(number, "invalid input row width or symbol")
        if len(outputs) != declarations[".o"] or set(outputs) - set("01-~"):
            error(number, "invalid output row width or symbol")
        if len(rows) == max_rows:
            error(number, "product row limit exceeded")
        rows.append((inputs, outputs))
    if pending:
        raise PlaError(f"incomplete {pending} labels")
    if not ended:
        raise PlaError("missing .e terminator")
    for required in (".type", ".i", ".o", ".ilb", ".ob", ".p"):
        if required not in declarations:
            raise PlaError(f"missing {required}")
    if declarations[".p"] != len(rows):
        raise PlaError(f"declared .p {declarations['.p']} but found {len(rows)} product rows")
    for key in (".ilb", ".ob"):
        labels = declarations[key]
        if len(set(labels)) != len(labels):
            raise PlaError(f"duplicate labels in {key}")
    return Pla(declarations[".ilb"], declarations[".ob"],
               declarations.get(".phase", "1" * declarations[".o"]),
               tuple(rows), device, hashlib.sha256(data).hexdigest())


def read_pla(path: Path | str) -> Pla:
    return parse_pla(Path(path).read_bytes())


class Bdd:
    """One unique table and variable order for both compared designs."""

    def __init__(self, names, max_nodes, max_work):
        if max_nodes < 0 or max_work < 0:
            raise PlaError("BDD budgets must be nonnegative")
        self.names = tuple(names)
        self.positions = {name: i for i, name in enumerate(names)}
        self.max_nodes, self.max_work = max_nodes, max_work
        self.nodes = [None, None]  # 0=false, 1=true.
        self.unique = {}
        self.or_cache = {}
        self.not_cache = {0: 1, 1: 0}
        self.work = 0

    def tick(self):
        if self.work == self.max_work:
            raise PlaError("BDD work budget exceeded; comparison is unresolved")
        self.work += 1

    def make(self, variable, low, high):
        self.tick()
        if low == high:
            return low
        key = (variable, low, high)
        if key not in self.unique:
            if len(self.unique) == self.max_nodes:
                raise PlaError("BDD node budget exceeded; comparison is unresolved")
            self.unique[key] = len(self.nodes)
            self.nodes.append(key)
        return self.unique[key]

    def variable(self, node):
        return self.nodes[node][0] if node > 1 else len(self.names)

    def either(self, a, b):
        self.tick()
        if a == 1 or b == 1:
            return 1
        if a == 0 or a == b:
            return b
        if b == 0:
            return a
        key = (min(a, b), max(a, b))
        if key in self.or_cache:
            return self.or_cache[key]
        variable = min(self.variable(a), self.variable(b))
        al, ah = self.nodes[a][1:] if self.variable(a) == variable else (a, a)
        bl, bh = self.nodes[b][1:] if self.variable(b) == variable else (b, b)
        result = self.make(variable, self.either(al, bl), self.either(ah, bh))
        self.or_cache[key] = result
        return result

    def complement(self, node):
        self.tick()
        if node not in self.not_cache:
            variable, low, high = self.nodes[node]
            result = self.make(variable, self.complement(low), self.complement(high))
            self.not_cache[node] = result
            self.not_cache[result] = node
        return self.not_cache[node]

    def cube(self, labels, values):
        literals = []
        for name, value in zip(labels, values):
            self.tick()
            if value != "-":
                literals.append((self.positions[name], value))
        literals.sort()
        result = 1
        for variable, value in reversed(literals):
            result = self.make(variable, 0, result) if value == "1" else self.make(variable, result, 0)
        return result

    def compile(self, pla):
        outputs = [0] * len(pla.outputs)
        for inputs, selections in pla.rows:
            self.tick()
            cube = self.cube(pla.inputs, inputs)
            for index, selection in enumerate(selections):
                self.tick()
                if selection == "1":
                    outputs[index] = self.either(outputs[index], cube)
        for index, phase in enumerate(pla.phase):
            if phase == "0":
                outputs[index] = self.complement(outputs[index])
        return dict(zip(pla.outputs, outputs))

    def witness(self, a, b):
        """Return one complete assignment distinguishing two BDD roots."""
        values = {name: False for name in self.names}
        while a != b and (a > 1 or b > 1):
            self.tick()
            variable = min(self.variable(a), self.variable(b))
            al, ah = self.nodes[a][1:] if self.variable(a) == variable else (a, a)
            bl, bh = self.nodes[b][1:] if self.variable(b) == variable else (b, b)
            if al != bl:
                a, b = al, bl
            else:
                values[self.names[variable]] = True
                a, b = ah, bh
        return values if a != b else None


def compare_pla(left: Pla, right: Pla, *, max_nodes=1_000_000,
                max_work=10_000_000) -> dict:
    names = sorted(set(left.inputs) | set(right.inputs))
    # Limit recursion depth independently of caller-constructed Pla objects.
    if len(names) > 256:
        raise PlaError("more than 256 distinct input variables")
    bdd = Bdd(names, max_nodes, max_work)
    lhs, rhs = bdd.compile(left), bdd.compile(right)
    shared = sorted(lhs.keys() & rhs.keys())
    different = [name for name in shared if lhs[name] != rhs[name]]
    left_only = sorted(lhs.keys() - rhs.keys())
    right_only = sorted(rhs.keys() - lhs.keys())
    functions_equal = not different and not left_only and not right_only
    device_compatible = left.device is None or right.device is None or left.device == right.device
    return {
        "equivalent": functions_equal and device_compatible,
        "functions_equivalent": functions_equal,
        "equivalent_functions": len(shared) - len(different),
        "output_counts": [len(lhs), len(rhs)],
        "different_outputs": different,
        "missing_in_left": right_only,
        "missing_in_right": left_only,
        "counterexamples": {name: bdd.witness(lhs[name], rhs[name]) for name in different[:8]},
        "devices": [left.device, right.device],
        "devices_compatible": device_compatible,
        "input_labels_equal": set(left.inputs) == set(right.inputs),
        "input_order_equal": left.inputs == right.inputs,
        "output_order_equal": left.outputs == right.outputs,
        "matrix_equal": (left.inputs, left.outputs, left.phase, left.rows) ==
                        (right.inputs, right.outputs, right.phase, right.rows),
        "bytes_equal": left.sha256 == right.sha256,
        "product_rows": [len(left.rows), len(right.rows)],
        "bdd_nodes": len(bdd.unique),
        "bdd_work": bdd.work,
        "scope": "Boolean output functions; placement and sequential behavior excluded",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    parser.add_argument("--max-nodes", type=int, default=1_000_000)
    parser.add_argument("--max-work", type=int, default=10_000_000)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = compare_pla(read_pla(args.left), read_pla(args.right),
                             max_nodes=args.max_nodes, max_work=args.max_work)
    except (OSError, PlaError) as exc:
        print(f"compare_pla: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"Equivalent Boolean functions: {result['equivalent_functions']} "
              f"of {result['output_counts'][0]}/{result['output_counts'][1]}")
        for key in ("different_outputs", "missing_in_left", "missing_in_right"):
            if result[key]:
                print(f"{key}: {', '.join(result[key])}")
        print("Device identities:", "compatible" if result["devices_compatible"] else "different")
        print("Labeled matrix and cube order:", "equal" if result["matrix_equal"] else "different")
        print("File bytes:", "equal" if result["bytes_equal"] else "different")
        print(f"BDD: {result['bdd_nodes']} nodes, {result['bdd_work']} operations")
        print(result["scope"])
    return 0 if result["equivalent"] else 1


if __name__ == "__main__":
    sys.exit(main())
