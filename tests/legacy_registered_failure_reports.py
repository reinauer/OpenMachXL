#!/usr/bin/env python3
"""Compare complete registered and input-paired legacy failure reports."""
from pathlib import Path
import difflib
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from report_banner import without_banner

ROOT = Path(__file__).with_name('legacy-registered-failure-reports')
CASES = json.loads((ROOT / 'manifest.json').read_text())['cases']


def normalized(data):
    data = without_banner(data)
    data = re.sub(rb'^(Start:|End  :).*\r\n', rb'\1 <time>\r\n', data, flags=re.M)
    data = re.sub(rb'\* Design Name = [^,\r\n]*(, Device = [^,\r\n]*), [^\r\n]* \*',
                  rb'* Design Name = source.pds\1, <time> *', data)
    data = re.sub(rb'\* Design Name = [^,\r\n]* \*', b'* Design Name = source.pds *', data)
    return re.sub(rb'Design \[.*?\]', b'Design [source.pds]', data)


def compare(name, suffix, expected, actual):
    old, new = normalized(expected), normalized(actual)
    if old != new:
        difference = ''.join(difflib.unified_diff(old.decode().splitlines(True),
                                                 new.decode().splitlines(True)))
        raise AssertionError(f'{name}.{suffix}:\n{difference}')


for case in CASES:
    name, stage = case['name'], case['stage']
    folder = ROOT / name
    for filename, digest in case['sha256'].items():
        assert hashlib.sha256((folder / filename).read_bytes()).hexdigest() == digest, filename
    with tempfile.TemporaryDirectory(prefix='machxl-legacy-failure-') as temporary:
        reports = Path(temporary)
        for compatibility in (False, True):
            command = [sys.argv[1], '--emit-jedec', str(folder / (name + '.PDS')),
                       '--settings', str(folder / (name + '.MXL'))]
            if compatibility:
                command += ['--compatibility']
            plain = subprocess.run(command, capture_output=True, timeout=30)
            assert plain.returncode and not plain.stdout, name
            for suffix in ('RPT', 'PRD', 'TAL', 'HST', 'TRF'):
                (reports / f'{name}.{suffix}').write_text('stale')
            (reports / 'UNRELATED.RPT').write_text('keep')
            result = subprocess.run(command + ['--fitter-reports', temporary,
                '--simulate', '--simulation-reports', temporary], capture_output=True, timeout=30)
            assert result.returncode and not result.stdout and result.stderr == plain.stderr, name
            suffixes = ['RPT', 'TAL'] + ([] if stage == 'partition' else ['PRD'])
            assert sorted(p.name for p in reports.iterdir()) == sorted(
                [f'{name}.{suffix}' for suffix in suffixes] + ['UNRELATED.RPT']), name
            assert (reports / 'UNRELATED.RPT').read_text() == 'keep'
            for suffix in suffixes:
                original = (folder / f'{name}.{suffix}').read_bytes()
                actual = (reports / f'{name}.{suffix}').read_bytes()
                compare(name, suffix, original, actual)
print('7 registered and paired legacy failures in both modes: full RPT/PRD/TAL bytes, counters and report lifecycle')
