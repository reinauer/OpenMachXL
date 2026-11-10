#!/usr/bin/env python3
"""Run the unmodified DOS compiler as a development reference, not a backend."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[1]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def members(path):
    with ZipFile(path) as archive:
        result = {}
        for name in archive.namelist():
            if not re.fullmatch(r"[A-Za-z0-9_.-]+", name) or name in (".", ".."):
                raise ValueError(f"Unexpected archive path: {name!r}")
            result[name.upper()] = archive.read(name)
        return result


def prepare(dos, destination, example, examples):
    destination.mkdir(parents=True, exist_ok=False)
    for archives, folder in [(["EXE-0.ZIP", "EXE-1.ZIP", "EXE-2.ZIP"], "EXE"),
                             (["DAT.ZIP"], "DAT")]:
        directory = destination / folder
        directory.mkdir()
        for archive in archives:
            for name, data in members(dos / archive).items():
                (directory / name).write_bytes(data)
    work = destination / "WORK"
    work.mkdir()
    for name, data in examples.items():
        (work / name).write_bytes(data)
    stem = Path(example).stem
    settings_name = stem + ".MXL"
    settings = examples.get(settings_name, (destination / "DAT/SETUP.MXL").read_bytes())
    text = settings.decode("cp437")
    # Only host paths, selected input/log, and interactive controls change.
    overrides = {"EXE_DIR": "C:\\EXE", "WORK_DIR": "C:\\WORK",
                 "DESIGN_FILE": example, "RUN_LOG": stem + ".LOG",
                 "COMP_OPT": "N", "SIM_OPT": "N", "DESIGN_OPT": "N",
                 "BELL_OPT": "N", "SILENT_OPT": "Y"}
    for key, value in overrides.items():
        pattern = r"(?m)^\[" + key + r"\]=[^\r\n]*"
        if re.search(pattern, text):
            text = re.sub(pattern, lambda _: f"[{key}]={value}", text)
        else:
            text += f"\r\n[{key}]={value}\r\n"
    (work / settings_name).write_bytes(text.encode("cp437"))
    batch = ["@echo off", "set MACHXL=C:\\", "set PATH=C:\\EXE;Z:\\",
             "c:", "cd \\WORK", f"machxl -D -BR {settings_name} {example}",
             "echo DONE > C:\\DONE.TXT", "exit", ""]
    (destination / "RUN.BAT").write_bytes("\r\n".join(batch).encode("ascii"))
    if any(c in str(destination) for c in '\n\r"'):
        raise ValueError("Output path cannot contain a newline or double quote")
    config = ("[sdl]\noutput=surface\n[mixer]\nnosound=true\n[midi]\n"
              "mpu401=none\nmididevice=none\n[cpu]\ncore=normal\n"
              "cycles=fixed 50000\n[dosbox]\nmemsize=32\n[autoexec]\n"
              f'mount c "{destination}"\nc:\nRUN.BAT\n')
    (destination / "dosbox.conf").write_text(config)
    return {"source_sha256": sha256(examples[example]),
            "original_settings_sha256": sha256(settings),
            "settings_source": "EXAMPLES.ZIP/" + settings_name
            if settings_name in examples else "DAT.ZIP/SETUP.MXL",
            "settings_overrides": overrides,
            "effective_settings_sha256": sha256(text.encode("cp437"))}


def run_one(dos, output, example, examples, executable, timeout):
    destination = output / Path(example).stem.lower()
    report = {"example": example, "directory": str(destination),
              **prepare(dos, destination, example, examples)}
    command = [executable, "-conf", str(destination / "dosbox.conf"),
               "-defaultdir", str(destination), "-fastlaunch", "-nogui", "-nomenu"]
    report["command"] = command
    start = time.monotonic()
    with (destination / "host.log").open("wb") as log:
        try:
            process = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                     env={**os.environ, "SDL_VIDEODRIVER": "dummy",
                                          "SDL_AUDIODRIVER": "dummy"}, timeout=timeout)
            report["host_returncode"] = process.returncode
        except subprocess.TimeoutExpired:
            report["host_returncode"] = None
            report["timed_out"] = True
    report["elapsed_seconds"] = round(time.monotonic() - start, 3)
    stem = Path(example).stem
    log_path = destination / "WORK" / (stem + ".LOG")
    log = log_path.read_bytes().decode("cp437") if log_path.exists() else ""
    report["dos_batch_completed"] = (destination / "DONE.TXT").exists()
    report["error_counts"] = [int(n) for n in re.findall(r"ERROR count\s*:?\s*(\d+)", log)]
    report["fitting_successful"] = "Fitting successful." in log
    jedec = destination / "WORK" / (stem + ".JED")
    report["success"] = (report["host_returncode"] == 0 and report["dos_batch_completed"]
                         and report["fitting_successful"] and jedec.exists()
                         and bool(report["error_counts"]) and not any(report["error_counts"]))
    report["outputs"] = {p.name: {"sha256": sha256(p.read_bytes()), "bytes": p.stat().st_size}
                         for p in sorted((destination / "WORK").glob(stem + ".*"))}
    (destination / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("examples", nargs="*", help="Example stems; default: all bundled examples")
    parser.add_argument("--dos", type=Path, default=ROOT / "dos")
    parser.add_argument("--output", type=Path, required=True, help="New results directory")
    parser.add_argument("--dosbox", default="dosbox-x")
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args()
    executable = shutil.which(args.dosbox)
    if not executable:
        parser.error("DOSBox-X is required only for generating reference results")
    examples = members(args.dos / "EXAMPLES.ZIP")
    names = [Path(n).stem.upper() + ".PDS" for n in args.examples] if args.examples else sorted(
        name for name in examples if name.endswith(".PDS"))
    if len(names) != len(set(names)) or any(name not in examples for name in names):
        parser.error("Specify distinct bundled example names")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    version = subprocess.run([executable, "-version"], capture_output=True, text=True)
    version_text = (version.stdout + version.stderr).strip()
    if "DOSBox-X version" not in version_text:
        parser.error("Could not identify the selected DOSBox-X executable")
    manifest = {"purpose": "DOS reference only; not a native compiler",
                "dosbox_version": version_text,
                "archives": {p.name: sha256(p.read_bytes()) for p in
                             (args.dos / n for n in ["EXAMPLES.ZIP", "DAT.ZIP", "EXE-0.ZIP", "EXE-1.ZIP", "EXE-2.ZIP"])},
                "results": []}
    for name in names:
        report = run_one(args.dos, output, name, examples, executable, args.timeout)
        manifest["results"].append(report)
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(f'{name}: {"PASS" if report["success"] else "FAIL"} ({report["elapsed_seconds"]}s)', flush=True)
    return 0 if all(r["success"] for r in manifest["results"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
