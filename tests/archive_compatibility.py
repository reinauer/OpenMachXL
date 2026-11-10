#!/usr/bin/env python3
"""Require the complete archive, including its original fitting rejection."""
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from verify_compatibility import run

with tempfile.TemporaryDirectory(prefix="machxl-archive-") as directory:
    report = run(Path(sys.argv[1]).resolve(), ROOT / "tests/corpus",
                 Path(directory) / "results", 60)
    assert report["summary"]["all_archive_results_match"], report["summary"]
