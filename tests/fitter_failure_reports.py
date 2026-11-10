#!/usr/bin/env python3
"""Retained failed frames, with explicit checks for corrected DOS diagnostics."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from report_banner import without_banner

root = Path(__file__).resolve().parent / 'fitter-failure-reports'
records = json.loads((root / 'manifest.json').read_text())


def stable_prefix(data):
    # Compare the original summary, fanout and pin tables, including the
    # actual logical-walk counters. Only the infeasible CSM summary differs.
    data = data.split(b'\t< Block [A] >')[0]
    data = without_banner(data)
    data = re.sub(rb'^(Start:|End  :).*\r\n', rb'\1 <time>\r\n', data, flags=re.M)
    data = re.sub(rb'Design \[.*?\]', b'Design [source.pds]', data)
    data = re.sub(rb'\(\d+ of (\d+) signals routed\)', rb'(<routed> of \1 signals routed)', data)
    return data


for record in records:
    name = record['name']
    case = root / name
    for filename, digest in record['sha256'].items():
        assert hashlib.sha256((case / filename).read_bytes()).hexdigest() == digest
    expected = (case / (name + '.PRD')).read_bytes()
    with tempfile.TemporaryDirectory(prefix='machxl-failed-fit-') as temp:
        destination = Path(temp)
        for compatibility in (False, True):
            command = [sys.argv[1], '--emit-jedec', str(case / (name + '.PDS')),
                       '--settings', str(case / (name + '.MXL'))]
            if compatibility:
                command.append('--compatibility')
            plain = subprocess.run(command, capture_output=True, timeout=30)
            assert plain.returncode and not plain.stdout, (name, plain)
            for suffix in ('.RPT', '.PRD', '.TAL', '.HST', '.TRF'):
                (destination / (name + suffix)).write_text('stale successful output')
            (destination / 'OTHER.RPT').write_text('unrelated report')
            result = subprocess.run(command + ['--fitter-reports', temp, '--simulate',
                                    '--simulation-reports', temp], capture_output=True, timeout=30)
            assert result.returncode and not result.stdout, (name, result)
            assert result.stderr == plain.stderr, (name, 'reporting changed failure diagnostic', result.stderr)
            assert sorted(p.name for p in destination.iterdir()) == sorted([
                name + '.PRD', name + '.RPT', name + '.TAL', 'OTHER.RPT'])
            assert (destination / 'OTHER.RPT').read_text() == 'unrelated report'
            actual = (destination / (name + '.PRD')).read_bytes()
            assert stable_prefix(actual) == stable_prefix(expected), (name, 'original failure-summary tables')
            # DOS says 6/6 or 19/19 despite IMX congestion and an incomplete
            # CSM table. Report only the native trial's feasible assignments.
            assigned, total = map(int, re.search(rb'\((\d+) of (\d+) signals routed\)', actual).groups())
            assert assigned + 1 == total == (6 if name == 'MRW4' else 19), name
            routed_names = re.findall(rb'^Mux\d+\| +([A-Z][A-Z0-9_]*) +(?:IO Pin|RegInp| Mcell| Input Pin)', actual, re.M)
            assert len(routed_names) == assigned and len(set(routed_names)) == assigned, name
            assert b'A' not in routed_names, (name, 'unavailable raw input was presented as routed')
            assert b'Maximum PT Capacity' not in actual, name
            resource = (destination / (name + '.RPT')).read_bytes()
            assert resource.endswith(b'%%% Fitting process has failed %%%\r\n'), name
            assert b'stale successful output' not in resource
            # Both fixed and searched failed placements now preserve the
            # original final physical trial; only CSM feasibility is corrected.
            original_sections = expected.split(b'\t< Block [A] >')[1:]
            actual_sections = actual.split(b'\t< Block [A] >')[1:]
            assert len(original_sections) == len(actual_sections)
            for old, new in zip(original_sections, actual_sections):
                if old.startswith(b'\tLogic Array Fan-in'):
                    continue
                assert old == new, (name, 'final failed placement table differs')
print(f'{len(records)} failed physical fits in both modes: original summary/placement formats and corrected retained routes; lifecycle preserved')
