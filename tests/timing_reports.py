#!/usr/bin/env python3
"""Compare structural TAL reports with captured unmodified DOS output."""
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent / 'timing-reports-reference'
records = json.loads((root / 'manifest.json').read_text())

def expected(record):
    name = record['name']
    data = (root / name / (name + '.TAL')).read_bytes()
    # MACHSP and direct MACHFITR choose different filename case. The native
    # API takes the caller's basename; this is the sole normalization.
    return re.sub(rb'(?m)^\* Design Name = [^\r\n]* \*',
                  b'* Design Name = ' + name.encode() + b'.PDS *', data)

for record in records:
    name = record['name']
    case = root / name
    for filename, digest in record['sha256'].items():
        assert hashlib.sha256((case / filename).read_bytes()).hexdigest() == digest, filename
    result = subprocess.run([sys.argv[1], str(case / (name + '.PDS')),
                             str(case / (name + '.MXL'))], capture_output=True)
    want = expected(record)
    if result.returncode or result.stdout != want:
        difference = ''.join(difflib.unified_diff(
            want.decode('ascii').splitlines(True),
            result.stdout.decode('ascii').splitlines(True)))
        raise AssertionError(f'{name}: {result.stderr.decode()}\n{difference}')

if len(sys.argv) > 2:
    with tempfile.TemporaryDirectory(prefix='machxl-timing-') as temp:
        for name in ('3COUNT_D', 'DATA_AQ', 'NOT2BIG'):
            record = next(r for r in records if r['name'] == name)
            case = root / name
            reports = Path(temp) / name
            if name == 'NOT2BIG':
                reports.mkdir()
                for suffix in ('.RPT', '.PRD', '.TAL', '.HST', '.TRF'):
                    (reports / (name + suffix)).write_text('stale successful report')
                (reports / 'OTHER.RPT').write_text('unrelated report')
                (reports / 'OTHER.HST').write_text('unrelated history')
            command = [sys.argv[2], '--emit-jedec', '--compatibility',
                '--settings', str(case / (name + '.MXL')), '--fitter-reports',
                str(reports), str(case / (name + '.PDS'))]
            if name == 'NOT2BIG':
                command += ['--simulate', '--simulation-reports', str(reports)]
            result = subprocess.run(command, capture_output=True)
            if name == 'NOT2BIG':
                assert result.returncode and not result.stdout, result
            else:
                assert result.returncode == 0, result.stderr.decode()
            actual = reports / (name + '.TAL')
            assert actual.read_bytes() == expected(record), f'{name} CLI TAL mismatch'
            if name == 'NOT2BIG':
                assert sorted(p.name for p in reports.iterdir()) == ['NOT2BIG.RPT', 'NOT2BIG.TAL', 'OTHER.HST', 'OTHER.RPT']
                assert (reports / 'OTHER.RPT').read_text() == 'unrelated report'
                assert (reports / 'OTHER.HST').read_text() == 'unrelated history'
            else:
                baseline = subprocess.run([sys.argv[2], '--emit-jedec', '--compatibility',
                    '--settings', str(case / (name + '.MXL')),
                    str(case / (name + '.PDS'))], capture_output=True)
                assert baseline.returncode == 0 and baseline.stdout == result.stdout
        source = root / '3COUNT_D' / '3COUNT_D.PDS'
        blocker = Path(temp) / 'ordinary-file'
        blocker.write_text('preserve existing contents')
        result = subprocess.run([sys.argv[2], '--emit-jedec', '--fitter-reports',
            str(blocker), str(source)], capture_output=True)
        assert result.returncode and not result.stdout
        assert blocker.read_text() == 'preserve existing contents'
        invalid = Path(temp) / 'invalid-request'
        result = subprocess.run([sys.argv[2], '--emit-pla', '--fitter-reports',
            str(invalid), str(source)], capture_output=True)
        assert result.returncode and not result.stdout and not invalid.exists()

        # Report destinations must never replace any compilation input,
        # including an input with an unconventional extension or an alias.
        settings = root / '3COUNT_D' / '3COUNT_D.MXL'
        collision = Path(temp) / 'collision'
        collision.mkdir()
        unusual_source = collision / 'SOURCE.RPT'
        unusual_source.write_bytes(source.read_bytes())
        before = unusual_source.read_bytes()
        result = subprocess.run([sys.argv[2], '--emit-jedec',
            str(unusual_source), '--settings', str(settings), '--fitter-reports',
            str(collision)], capture_output=True)
        assert result.returncode and not result.stdout
        assert unusual_source.read_bytes() == before

        normal_source = collision / 'SETTINGS.PDS'
        normal_source.write_bytes(source.read_bytes())
        unusual_settings = collision / 'SETTINGS.HST'
        unusual_settings.write_bytes(settings.read_bytes())
        before = unusual_settings.read_bytes()
        result = subprocess.run([sys.argv[2], '--emit-jedec', str(normal_source),
            '--settings', str(unusual_settings), '--simulate',
            '--simulation-reports', str(collision)], capture_output=True)
        assert result.returncode and not result.stdout
        assert unusual_settings.read_bytes() == before

        normal_source = collision / 'PLACEMENT.PDS'
        normal_source.write_bytes(source.read_bytes())
        placement = collision / 'saved.plc'
        placement.write_text('; No placement overrides.\n')
        alias = collision / 'PLACEMENT.PRD'
        alias.symlink_to(placement.name)
        before = placement.read_bytes()
        # Preflight must check every output before deleting the first one.
        earlier_output = collision / 'PLACEMENT.RPT'
        earlier_output.write_text('preserve on rejected report configuration')
        result = subprocess.run([sys.argv[2], '--emit-jedec', str(normal_source),
            '--settings', str(settings), '--placement', str(placement),
            '--fitter-reports', str(collision)], capture_output=True)
        assert result.returncode and not result.stdout
        assert placement.read_bytes() == before and alias.is_symlink()
        assert earlier_output.read_text() == 'preserve on rejected report configuration'
print(f'{len(records)} original TAL reports match; CLI success/failure integration passed')
