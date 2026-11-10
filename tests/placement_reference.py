#!/usr/bin/env python3
"""Compare user PLC/BLC constraints with original standalone MACHFITR."""
import hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'));import jedec
compiler=Path(sys.argv[1]).resolve();fixtures=ROOT/'tests/placement-reference'
for record in json.loads((fixtures/'manifest.json').read_text())['cases']:
 for filename,digest in record['files'].items():
  assert hashlib.sha256((fixtures/filename).read_bytes()).hexdigest()==digest,filename
 name=record['name'];source=fixtures/(name+'.PDS');settings=fixtures/(name+'.MXL');placement=fixtures/(name+'.PLC')
 for flags in ([],['--compatibility']):
  result=subprocess.run([str(compiler),'--emit-jedec',str(source),'--settings',str(settings),'--placement',str(placement),*flags],capture_output=True,timeout=60)
  if not record['success']:
   assert result.returncode and not result.stdout,(name,flags,'expected rejection');continue
  assert result.returncode==0,(name,flags,result.stderr.decode())
  comparison=jedec.compare(jedec.parse((fixtures/(name+'.JED')).read_bytes()),jedec.parse(result.stdout))
  assert comparison['equal'],(name,flags,comparison)
 # Corrected FITR_PLACE reads the sibling PLC. Compatibility retains the
 # MACHSP filename omission, while explicit --placement above always works.
 if record['success']:
  corrected=subprocess.run([str(compiler),'--emit-jedec',str(source),'--settings',str(settings)],capture_output=True,timeout=60)
  assert corrected.returncode==0,(name,corrected.stderr.decode())
  assert jedec.compare(jedec.parse((fixtures/(name+'.JED')).read_bytes()),jedec.parse(corrected.stdout))['equal'],name
  if not record.get("check_omission",True):continue
  compatible=subprocess.run([str(compiler),'--emit-jedec',str(source),'--settings',str(settings),'--compatibility'],capture_output=True,timeout=60)
  assert compatible.returncode==0,(name,compatible.stderr.decode())
  assert jedec.compare(jedec.parse((fixtures/'SOURCE.JED').read_bytes()),jedec.parse(compatible.stdout))['equal'],name
print('Original placement files: explicit and project selection verified')
