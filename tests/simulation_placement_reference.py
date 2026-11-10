#!/usr/bin/env python3
"""Compare original simulation placement, fuse maps, and vector suppression."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import jedec

compiler=Path(sys.argv[1]).resolve()
fixtures=ROOT/'tests/simulation-placement-reference'
for case in json.loads((fixtures/'manifest.json').read_text())['cases']:
    name=case['name']
    for filename,digest in case['files'].items():
        assert hashlib.sha256((fixtures/filename).read_bytes()).hexdigest()==digest,filename
    expected=jedec.parse((fixtures/case['reference']).read_bytes())
    for flags in [[],['--compatibility']]:
        result=subprocess.run([str(compiler),'--emit-jedec',str(fixtures/(name+'.PDS')),
            '--settings',str(fixtures/(name+'.MXL')),'--simulate',*flags],capture_output=True,timeout=60)
        assert not result.returncode,(name,flags,result.stderr.decode())
        comparison=jedec.compare(expected,jedec.parse(result.stdout))
        assert comparison['equal'] and all(comparison['transmission_verified']),(name,flags,comparison)
        assert b'CHECK mismatch'not in result.stderr,(name,flags,result.stderr)
        if case.get('warning'):
            assert case['warning'].encode()in result.stderr,(name,flags,'missing suppression diagnostic')
    print(name,'exact original simulation placement and JEDEC')
