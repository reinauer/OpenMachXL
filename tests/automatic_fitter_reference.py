#!/usr/bin/env python3
"""Check complete native placement/routing against archived DOS results."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import jedec

compiler = Path(sys.argv[1]).resolve()
reference = ROOT / 'tests/corpus'
manifest = json.loads((reference / 'manifest.json').read_text())
names = {'3COUNT_D', '3COUNT_T', 'BARREL', 'CNTCASE', 'CNTIF', 'COUNTER', 'DECODE', 'LRSHIFT', 'MOORE_S', 'UDCNT', 'TEST', '16CNTMUX', 'P1_SRC', 'MOORE_B', 'LATCH', 'P7_SRC', 'P8_SRC', 'TEST2', 'P1_SRN', 'COMPARA', 'MOORE_C', 'MUX', 'P3_SRC', 'P4_SRC', 'P9_SRC', 'P5_SRC', 'P6_SRC', 'P2_SRC', 'DB', 'DATA_AQ', 'PF_SRC', 'TEST3'}
with tempfile.TemporaryDirectory() as directory, ZipFile(ROOT / 'dos/EXAMPLES.ZIP') as archive:
    temporary = Path(directory)
    for case in manifest['cases']:
        name = case['name']
        if name not in names:
            continue
        source = archive.read(name + '.PDS')
        assert hashlib.sha256(source).hexdigest() == case['source_sha256']
        settings_archive, member = case['settings_source'].split('/', 1)
        with ZipFile(ROOT / 'dos' / settings_archive) as settings_zip:
            settings = settings_zip.read(member)
        assert hashlib.sha256(settings).hexdigest() == case['original_settings_sha256']
        expected = (reference / name / (name + '.JED')).read_bytes()
        assert hashlib.sha256(expected).hexdigest() == case['files'][name + '.JED']['sha256']
        original = jedec.parse(expected)
        pds, mxl = temporary / (name + '.PDS'), temporary / (name + '.MXL')
        pds.write_bytes(source)
        mxl.write_bytes(settings)
        # P9 exercises the confirmed DOS CASE high-word formatting bug.
        modes = (['--compatibility'],) if name == 'P9_SRC' else ([], ['--compatibility'])
        for flags in modes:
            result = subprocess.run([str(compiler), *flags, '--settings', str(mxl),
                                     '--emit-jedec', str(pds)], capture_output=True, timeout=60)
            assert result.returncode == 0, (name, flags, result.stderr)
            image = jedec.parse(result.stdout)
            comparison = jedec.compare(original, image)
            assert comparison['equal'] and image.transmission_verified, (name, flags, comparison)
        print(name + ': native placement, routing and all JEDEC bits match DOS'
              + (' in compatibility mode' if name == 'P9_SRC' else ' in both modes'))
