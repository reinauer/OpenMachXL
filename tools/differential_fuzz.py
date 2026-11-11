#!/usr/bin/env python3
"""Compare machxl with the original DOS compiler on random small designs.

Each design is generated from a seed, compiled by the original under DOSBox-X
(through tools/dos_oracle.py) and by machxl, and the results are compared:

  - both reject, or both accept
  - the PLA matrices are identical row for row (--compatibility and default)
  - the JEDEC fuse maps are equal

A mismatch is not a verdict. It is either a machxl bug or a defect of the
original that belongs behind --compatibility; each one needs a look. The
designs and the original's output files stay in the output directory.

    tools/differential_fuzz.py --compiler build/machxl --output fuzz-out \
        --count 40 --seed 1

Needs the original archives in dos/ and dosbox-x on PATH. A run of the
original takes about twenty seconds.
"""
import argparse
import json
import random
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import dos_oracle  # noqa: E402
import jedec  # noqa: E402
from compare_pla import compare_pla, parse_pla, read_pla  # noqa: E402

DEVICES = ["MACH111", "MACH211", "MACH215", "MACH231", "MACH355", "MACH435", "MACH445", "MACH465", "MACH120",
           "MACH210", "MACH220"]
# Every device the original fits; --devices may name any of them.
ALL_DEVICES = DEVICES + ["MACH110", "MACH121", "MACH130", "MACH131", "MACH221", "MACH446"]


def product(rng, signals):
    literals = rng.sample(signals, rng.randint(1, min(3, len(signals))))
    return "*".join(("/" if rng.random() < 0.35 else "") + name for name in literals)


def sum_of_products(rng, signals):
    return " + ".join(product(rng, signals) for _ in range(rng.randint(1, 3)))


def expression(rng, signals):
    """A sum of products, sometimes with parentheses or an exclusive OR."""
    kind = rng.random()
    if kind < 0.7:
        return sum_of_products(rng, signals)
    if kind < 0.85:
        return "(%s) * (%s)" % (sum_of_products(rng, signals), sum_of_products(rng, signals))
    return "%s :+: %s" % (product(rng, signals), product(rng, signals))


def equations_design(rng, name, device, scale=1):
    """Combinatorial and registered outputs, buried nodes, controls, IF and CASE blocks."""
    inputs = ["I%d" % i for i in range(rng.randint(2, min(6 * scale, 14)))]
    lines = ["CHIP %s %s" % (name, device), "PIN ? CLK"] + ["PIN ? %s" % i for i in inputs]
    equations, readable = [], list(inputs)
    outputs = []
    for index in range(rng.randint(1, 5 * scale)):
        node = index > 0 and rng.random() < 0.2
        signal = ("N%d" if node else "O%d") % index
        storage = rng.choice(["", "", " REG", " REG", " LATCHED"]) if not node else rng.choice(["", " REG"])
        # MACH1 has no latches; keep that rejection rare rather than typical.
        if storage == " LATCHED" and device[4] == "1" and rng.random() < 0.9:
            storage = " REG"
        lines.append("%s ? %s%s" % ("NODE" if node else "PIN", signal, storage))
        outputs.append((signal, storage, node))
    for signal, storage, node in outputs:
        assign = "=" if not storage else rng.choice(["=", ":="]) if storage == " REG" else rng.choice(["=", "*="])
        equations.append("%s %s %s" % (signal, assign, expression(rng, readable)))
        if storage:
            if rng.random() < 0.85:
                # Product-term clocks exist on MACH215 and MACH3/4 only; elsewhere
                # they are a (rare) rejection both compilers must agree on.
                modern = device in ("MACH215", "MACH355", "MACH435", "MACH445", "MACH465")
                plain = rng.random() < (0.7 if modern else 0.97)
                equations.append("%s.CLKF = %s" % (signal, "CLK" if plain else "*".join(rng.sample(inputs, 2))))
            if rng.random() < 0.3:
                equations.append("%s.RSTF = %s" % (signal, product(rng, inputs)))
            if rng.random() < 0.2:
                equations.append("%s.SETF = %s" % (signal, product(rng, inputs)))
        if not node and rng.random() < 0.2:
            equations.append("%s.TRST = %s" % (signal, product(rng, inputs)))
        readable.append(signal)
    # Every node must be read, or the original discards it; mostly make sure.
    pads = [signal for signal, _, node in outputs if not node]
    for signal, _, node in outputs:
        if node and pads and rng.random() < 0.8:
            target = "X%s" % signal
            lines.append("PIN ? %s" % target)
            equations.append("%s = %s * %s" % (target, signal, rng.choice(inputs)))
    block = rng.random()
    if block < 0.2 and len(inputs) >= 2:
        lines.append("PIN ? B0")
        equations += ["IF (%s) THEN" % product(rng, inputs), "BEGIN", "B0 = %s" % sum_of_products(rng, inputs),
                      "END", "ELSE", "BEGIN", "B0 = %s" % sum_of_products(rng, inputs), "END"]
    elif block < 0.4 and len(inputs) >= 3:
        lines.append("PIN ? C0")
        equations += ["CASE (%s,%s)" % (inputs[0], inputs[1]), "BEGIN"]
        for value in rng.sample(range(4), rng.randint(2, 4)):
            equations += ["%d: BEGIN C0 = %s END" % (value, sum_of_products(rng, inputs[2:]))]
        if rng.random() < 0.5:
            equations += ["OTHERWISE: BEGIN C0 = %s END" % sum_of_products(rng, inputs[2:])]
        equations += ["END"]
    return "\r\n".join(lines + ["EQUATIONS"] + equations) + "\r\n"


def state_design(rng, name, device):
    """A small state machine with explicit codes, optional outputs and default branch.

    A branch condition is a signal or a CONDITIONS name, optionally complemented,
    and the conditions of one equation must exclude each other; the generator
    mostly keeps to that and now and then breaks it, to compare the rejections.
    """
    bits = rng.randint(1, 2)
    states = ["S%d" % i for i in range(rng.randint(2, 1 << bits))]
    inputs = ["I%d" % i for i in range(rng.randint(1, 3))]
    lines = ["CHIP %s %s" % (name, device), "PIN ? CLK"] + ["PIN ? %s" % i for i in inputs]
    lines += ["PIN ? Q%d REG" % b for b in range(bits)]
    moore = rng.random() < 0.6
    has_output = rng.random() < 0.6
    if has_output:
        lines.append("PIN ? Z" + (" REG" if moore and rng.random() < 0.5 else ""))
    conditions = {}
    if len(inputs) >= 2 and rng.random() < 0.5:
        first, second = rng.sample(inputs, 2)
        conditions = {"G0": "%s*%s" % (first, second), "G1": "%s*/%s" % (first, second), "G2": "/%s" % first}

    def branch_conditions():
        """Two conditions for one equation: exclusive by construction, or rarely not."""
        roll = rng.random()
        if roll < 0.08:
            return [product(rng, inputs)]                       # an expression: P88
        if roll < 0.16 and len(inputs) >= 2:
            return rng.sample(inputs, 2)                        # overlapping: E1250/E1260
        if conditions and roll < 0.55:
            return rng.sample(sorted(conditions), rng.randint(1, 3))
        signal = rng.choice(inputs)
        return [signal, "/" + signal][:rng.randint(1, 2)] if rng.random() < 0.7 else ["/" + signal]

    body = ["STATE", "MOORE_MACHINE" if moore else "MEALY_MACHINE"]
    default = rng.random()
    if default < 0.3:
        body.append("DEFAULT_BRANCH HOLD_STATE")
    elif default < 0.5:
        body.append("DEFAULT_BRANCH %s" % rng.choice(states))
    elif default < 0.6:
        body.append("DEFAULT_BRANCH NEXT_STATE")
    body.append("CLKF = CLK")
    for index, state in enumerate(states):
        body.append("%s = %s" % (state, "*".join(("" if index >> b & 1 else "/") + "Q%d" % b for b in range(bits))))
    for state in states:
        arms = ["%s -> %s" % (condition, rng.choice(states)) for condition in branch_conditions()]
        transition = "%s := %s" % (state, "\r\n        + ".join(arms))
        if rng.random() < 0.5:
            transition += "\r\n        +-> %s" % rng.choice(states)
        body.append(transition)
    if has_output:
        for state in states:
            if rng.random() < 0.7:
                if moore:
                    body.append("%s.OUTF = %s" % (state, rng.choice(["Z", "/Z"])))
                else:
                    arms = ["%s -> %s" % (condition, rng.choice(["Z", "/Z"])) for condition in branch_conditions()]
                    body.append("%s.OUTF = %s" % (state, "\r\n        + ".join(arms)))
    if conditions:
        body += ["CONDITIONS"] + ["%s = %s" % item for item in sorted(conditions.items())]
    return "\r\n".join(lines + body) + "\r\n"


def generate(seed, index, scale=1, devices=None):
    rng = random.Random("%d/%d" % (seed, index) if scale == 1 else "%d/%d/%d" % (seed, index, scale))
    name = "F%05d" % (index % 100000)
    device = rng.choice(DEVICES)
    if devices:
        # The default draw above stays, so a seed means the same design body.
        device = devices[index % len(devices)]
    if rng.random() < 0.25:
        return name, device, state_design(rng, name, device)
    return name, device, equations_design(rng, name, device, scale)


def native(compiler, mode, source, settings, compatibility):
    command = [str(compiler), mode, str(source), "--settings", str(settings)]
    return subprocess.run(command + (["--compatibility"] if compatibility else []), capture_output=True, timeout=600)


def diagnostic(result):
    lines = [line for line in result.stderr.decode(errors="replace").splitlines()
             if ": info: " not in line and ": warning: " not in line]
    return lines[-1].split(": ", 1)[-1][:120] if lines else ""


def compare(compiler, work, name):
    """Returns the list of differences between the original's files in `work` and machxl."""
    source, settings = work / (name + ".PDS"), work / (name + ".MXL")
    log = (work / (name + ".LOG")).read_bytes().decode("cp437") if (work / (name + ".LOG")).exists() else ""
    errors = re.findall(r"\|> (?:ERROR|FATAL ERROR) ?([A-Za-z]?\d+)", log)
    differences = []
    for compatibility in (False, True):
        tag = "compatibility" if compatibility else "default"
        original_pla = work / (name + ".PLA")
        result = native(compiler, "--emit-pla", source, settings, compatibility)
        if original_pla.exists():
            if result.returncode:
                differences.append("%s: PLA rejected by machxl (%s)" % (tag, diagnostic(result)))
            else:
                matrices = compare_pla(parse_pla(result.stdout), read_pla(original_pla))
                if not matrices["matrix_equal"]:
                    differences.append("%s: PLA %s" % (tag, "equivalent, rows differ" if matrices["equivalent"]
                                                       else "is a different function"))
        result = native(compiler, "--emit-jedec", source, settings, compatibility)
        original_jedec = work / (name + ".JED")
        if original_jedec.exists():
            if result.returncode:
                differences.append("%s: JEDEC rejected by machxl (%s)" % (tag, diagnostic(result)))
            elif not jedec.compare(jedec.parse(original_jedec.read_bytes()), jedec.parse(result.stdout))["equal"]:
                differences.append("%s: JEDEC fuses differ" % tag)
        elif not result.returncode:
            differences.append("%s: accepted by machxl, original reports %s" % (tag, ",".join(errors) or "no JEDEC"))
    return differences, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--compiler", type=Path, required=True, help="machxl executable")
    parser.add_argument("--output", type=Path, required=True, help="new directory for designs and results")
    parser.add_argument("--count", type=int, default=20)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--start", type=int, default=0, help="index of the first design of this seed")
    parser.add_argument("--scale", type=int, default=1, choices=range(1, 5),
                        help="size factor for the number of inputs and outputs of equation designs")
    parser.add_argument("--devices", help="comma-separated devices to cycle through instead of the default mix")
    parser.add_argument("--dos", type=Path, default=ROOT / "dos", help="directory with the original archives")
    parser.add_argument("--timeout", type=int, default=300, help="seconds per run of the original")
    args = parser.parse_args()
    devices = [d.strip().upper() for d in args.devices.split(",")] if args.devices else None
    if devices and any(d not in ALL_DEVICES for d in devices):
        sys.exit("unknown device in --devices; known: " + " ".join(ALL_DEVICES))
    executable = shutil.which("dosbox-x")
    if not executable:
        sys.exit("dosbox-x not found on PATH")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    results = []
    for index in range(args.start, args.start + args.count):
        name, device, source = generate(args.seed, index, args.scale, devices)
        examples = {name + ".PDS": source.encode("ascii")}
        run = dos_oracle.run_one(args.dos, output, name + ".PDS", examples, executable, args.timeout)
        work = output / name.lower() / "WORK"
        if not run["dos_batch_completed"]:
            differences, errors = ["the original did not finish"], []
        else:
            differences, errors = compare(args.compiler.resolve(), work, name)
        results.append({"name": name, "seed": args.seed, "index": index, "scale": args.scale, "device": device,
                        "original_errors": errors, "differences": differences})
        print("%s %-8s %s" % (name, device, "; ".join(differences) if differences
                              else "same" + (" (both reject: %s)" % ",".join(errors) if errors else "")), flush=True)
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    mismatched = [r for r in results if r["differences"]]
    print("%d designs, %d with differences" % (len(results), len(mismatched)))
    return 1 if mismatched else 0


if __name__ == "__main__":
    sys.exit(main())
