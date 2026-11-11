#!/usr/bin/env python3
"""Compare frontend results with original compiler PLAs and rejections.

The optional second argument selects another fixture directory.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compare_pla import compare_pla, parse_pla, read_pla

compiler = Path(sys.argv[1]).resolve()
fixtures = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else ROOT / 'tests/state-default-reference'
for case in json.loads((fixtures / 'manifest.json').read_text())['cases']:
    name = case['name']
    for filename, digest in case['files'].items():
        assert hashlib.sha256((fixtures / filename).read_bytes()).hexdigest() == digest, filename
    for flags in ([], ['--compatibility']):
        result = subprocess.run([str(compiler), '--emit-pla', str(fixtures / (name + '.PDS')),
                                 '--settings', str(fixtures / (name + '.MXL')), *flags],
                                capture_output=True, timeout=60)
        if 'error' in case:
            # The original stops in MACHEXP; its log holds the diagnostic.
            assert case['error'].encode() in (fixtures / (name + '.LOG')).read_bytes(), name
            assert result.returncode and not result.stdout, (name, flags, 'original rejection accepted')
            assert case['error'].encode() in result.stderr, (name, flags, result.stderr)
            continue
        assert result.returncode == 0, (name, flags, result.stderr)
        comparison = compare_pla(parse_pla(result.stdout), read_pla(fixtures / (name + '.PLA')))
        assert comparison['equivalent'] and comparison['matrix_equal'], (name, flags, comparison)
        if case.get('header'):
            # The header text the original copies into its PLA must be the same.
            # It also writes a line for each header the design does not have.
            def header(data):
                lines = [line.rstrip() for line in data.decode('latin1').replace('\r', '').split('\n')]
                return [line for line in lines if len(line.split(' ')) > 2 and line.split(' ')[0] == '#AMD' and
                        line.split(' ')[1] in ('TITLE', 'PATTERN', 'REVISION', 'AUTHOR', 'COMPANY', 'DATE')]
            assert header(result.stdout) == header((fixtures / (name + '.PLA')).read_bytes()), (name, flags)
    print(name + (': original rejection reproduced' if 'error' in case else ': exact labeled matrix'))
