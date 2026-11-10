#!/usr/bin/env python3
"""Require original fitter rejection cases to fail without partial JEDEC."""
import hashlib,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];fixtures=ROOT/'tests/fitter-rejections-reference'
compiler=Path(sys.argv[1]).resolve()
for case in json.loads((fixtures/'manifest.json').read_text())['cases']:
 name=case['name']
 for filename,digest in case['files'].items():
  assert hashlib.sha256((fixtures/filename).read_bytes()).hexdigest()==digest,filename
 for flags in [[],['--compatibility']]:
  result=subprocess.run([str(compiler),'--emit-jedec',str(fixtures/(name+'.PDS')),'--settings',str(fixtures/(name+'.MXL')),*flags],capture_output=True,timeout=60)
  assert result.returncode and not result.stdout and result.stderr,(name,flags,'original rejection emitted output')
  with tempfile.TemporaryDirectory(prefix='machxl-rejection-reports-') as reports:
   observed=subprocess.run([str(compiler),'--emit-jedec',str(fixtures/(name+'.PDS')),'--settings',str(fixtures/(name+'.MXL')),*flags,'--fitter-reports',reports],capture_output=True,timeout=60)
   assert observed.returncode and not observed.stdout,(name,flags,'failed report emitted programming output')
   assert observed.stderr==result.stderr,(name,flags,'report capture changed rejection',observed.stderr,result.stderr)
print('Original fitter rejection cases fail before JEDEC output; report capture preserves diagnostics in both modes')
