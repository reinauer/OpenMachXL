#!/usr/bin/env python3
"""Check the legacy 18-term XOR rejection and corrected maps."""
import hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import jedec
compiler=Path(sys.argv[1]).resolve()
fixtures=ROOT/'tests/fitter-xor-descriptor-reference'
for case in json.loads((fixtures/'manifest.json').read_text())['cases']:
 name=case['name']
 for filename,digest in case['files'].items():
  assert hashlib.sha256((fixtures/filename).read_bytes()).hexdigest()==digest,filename
 original=(fixtures/(name+'.LOG')).read_bytes()
 assert b'z8004' in original and b'z8007' in original,name
 command=[str(compiler),'--emit-jedec',str(fixtures/(name+'.PDS')),'--settings',str(fixtures/(name+'.MXL'))]
 compatible=subprocess.run(command+['--compatibility'],capture_output=True,timeout=30)
 assert compatible.returncode and not compatible.stdout,name
 assert b'z8004' in compatible.stderr and b'z8007' in compatible.stderr,(name,compatible.stderr)
 corrected=subprocess.run(command,capture_output=True,timeout=30)
 assert not corrected.returncode,(name,corrected.stderr)
 expected=jedec.parse((fixtures/(name+'.CORRECTED.JED')).read_bytes())
 comparison=jedec.compare(jedec.parse(corrected.stdout),expected)
 assert comparison['equal'],(name,comparison)
 print(f'{name}: compatibility rejection and corrected reference map match')
