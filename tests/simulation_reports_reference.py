#!/usr/bin/env python3
"""Exact original history/trace bytes, from saved placement and native fit."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from report_banner import without_banner

ROOT = Path(__file__).resolve().parents[1]

def main():
    corpus = ROOT / 'tests/simulation-reports-corpus'
    manifest = json.loads((corpus / 'manifest.json').read_text())
    reports = fits = rejected = 0
    for case in manifest['cases']:
        name = case['name']
        folder = corpus / name
        for filename, digest in case['sha256'].items():
            assert hashlib.sha256((folder / filename).read_bytes()).hexdigest() == digest, filename
        source, plc, settings = [folder / (name + '.' + ext) for ext in ('PDS', 'PLC', 'MXL')]
        if not case['success']:
            process = subprocess.run([sys.argv[1], str(source), str(plc), str(settings), 'HST'], capture_output=True)
            assert process.returncode and b'D3230' in process.stderr, (name, process.stderr)
            rejected += 1
            continue
        for extension in ('HST', 'TRF'):
            original = folder / (name + '.' + extension)
            process = subprocess.run([sys.argv[1], str(source), str(plc), str(settings), extension], capture_output=True)
            assert process.returncode == 0, (name, process.stderr)
            expected = original.read_bytes() if original.exists() else b''
            assert without_banner(process.stdout) == without_banner(expected), (name, extension, 'saved-placement report differs')
            reports += original.exists()
        if case['native_fit']:
            with tempfile.TemporaryDirectory() as directory:
                process = subprocess.run([sys.argv[2], '--compatibility', '--emit-jedec', str(source), '--settings', str(settings), '--simulate', '--simulation-reports', directory], capture_output=True)
                assert process.returncode == 0, (name, process.stderr)
                for extension in ('HST', 'TRF'):
                    original = folder / (name + '.' + extension)
                    actual = Path(directory) / (name + '.' + extension)
                    if original.exists():
                        assert actual.exists() and without_banner(actual.read_bytes()) == without_banner(original.read_bytes()), (name, extension, 'native fitted report differs')
                    else:
                        assert not actual.exists(), (name, 'unexpected trace')
                fits += 1
    print(f'{reports} exact original HST/TRF files; {fits} native fit-and-simulate cases; {rejected} rejected clock commands')

if __name__ == '__main__':
    main()
