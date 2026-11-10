#!/usr/bin/env python3
"""Compare complete native fixed-pin images with fresh original DOS runs."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from jedec import read, compare

compiler = Path(sys.argv[1]).resolve()
corpus = ROOT / 'tests/fitter-corpus'
manifest = json.loads((corpus / 'manifest.json').read_text())
with tempfile.TemporaryDirectory() as directory:
    temporary = Path(directory)
    for case in manifest['cases']:
        name = case['name']
        for extension, digest in case['sha256'].items():
            path = corpus / (name + extension)
            if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                raise AssertionError(f'{path}: fixture changed')
        for settings in ([['--settings', str(corpus / (name + '.MXL'))]] if case.get('settings_required') else [[], ['--settings', str(corpus / (name + '.MXL'))]]):
            result = subprocess.run([str(compiler), '--compatibility', *settings, '--emit-jedec', str(corpus / (name + '.PDS'))], capture_output=True)
            if result.returncode:
                raise AssertionError(f'{name} {settings}: {result.stderr.decode()}')
            output = temporary / (name + '.JED')
            output.write_bytes(result.stdout)
            comparison = compare(read(corpus / (name + '.JED')), read(output))
            if not comparison['equal'] or not all(comparison['transmission_verified']):
                raise AssertionError(f'{name} {settings}: {comparison}')
        print(f'{name}: all 7724 main and 32 electrical bits match DOS, verified settings')

    unsupported = {
        'inverted_clock': 'PIN 3 A\nPIN 11 CLK\nPIN 2 Q REGISTERED\nEQUATIONS\nQ:=A\nQ.CLKF=/CLK',
        'constant': 'PIN 2 Q\nEQUATIONS\nQ=VCC',
        'multiple_resets': 'PIN 3 A\nPIN 4 B\nPIN 11 CLK\nPIN 5 Q REGISTERED\nPIN 6 R REGISTERED\nEQUATIONS\nQ:=A\nR:=B\nQ.CLKF=CLK\nR.CLKF=CLK\nQ.RSTF=A\nR.RSTF=B',
        'reset_sum': 'PIN 3 A\nPIN 4 B\nPIN 11 CLK\nPIN 2 Q REGISTERED\nEQUATIONS\nQ:=A\nQ.CLKF=CLK\nQ.RSTF=A+B',
    }
    for name, body in unsupported.items():
        source = temporary / (name + '.PDS')
        source.write_text('CHIP PROBE MACH111\n' + body + '\n')
        flags=['--compatibility'] if name=='constant' else []
        result = subprocess.run([str(compiler), '--emit-jedec', str(source), *flags], capture_output=True)
        if name=='constant':
            corrected=subprocess.run([str(compiler), '--emit-jedec', str(source)],capture_output=True)
            assert corrected.returncode==0 and corrected.stdout,corrected.stderr
        if result.returncode == 0 or result.stdout or not result.stderr:
            raise AssertionError(f'{name}: unsupported fitting did not fail cleanly')
        print(f'{name}: rejected before emitting JEDEC')

    for name in ('JY1112', 'JY1113', 'JC1110', 'JC1111', 'JC1113'):
        result = subprocess.run([str(compiler), '--emit-jedec', str(corpus / (name + '.PDS'))], capture_output=True)
        if result.returncode == 0 or result.stdout or not result.stderr:
            raise AssertionError(f'{name}: invalid output pairing did not fail cleanly')
        print(f'{name}: rejected invalid pairing like DOS')
