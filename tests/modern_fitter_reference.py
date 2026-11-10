#!/usr/bin/env python3
"""Compare native MACH3/4 fitting to immutable independent DOS images."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import jedec
compiler = Path(sys.argv[1]).resolve()
reference = Path(sys.argv[2]) if len(sys.argv)>2 else ROOT / 'tests/fitter-modern-reference'
manifest = json.loads((reference / 'manifest.json').read_text())
for case in manifest['cases']:
    name = case['name']
    for filename, digest in case['files'].items():
        assert hashlib.sha256((reference / filename).read_bytes()).hexdigest() == digest, filename
    original = jedec.parse((reference / (name + '.JED')).read_bytes())
    for flags in ([], ['--compatibility']):
        result = subprocess.run([str(compiler), '--emit-jedec', str(reference / (name + '.PDS')),
                                 '--settings', str(reference / (name + '.MXL')), *flags],
                                capture_output=True, timeout=60)
        assert result.returncode == 0, (name, flags, result.stderr)
        native = jedec.parse(result.stdout)
        comparison = jedec.compare(original, native)
        assert comparison['equal'] and native.transmission_verified, (name, flags, comparison)
    print(name + ': all native main, electrical and auxiliary JEDEC bits match DOS')
