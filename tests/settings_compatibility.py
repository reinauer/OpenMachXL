#!/usr/bin/env python3
"""Compare serialized DOS policy parsing and corrected native parsing."""
from pathlib import Path
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'));import jedec
compiler=Path(sys.argv[1]).resolve();fixtures=ROOT/'tests/settings-compatibility-reference'
records=json.loads((fixtures/'manifest.json').read_text())['cases']
for record in records:
    name=record['name']
    for filename,digest in record['files'].items():
        assert hashlib.sha256((fixtures/filename).read_bytes()).hexdigest()==digest,filename
    for compatibility in (True,False):
        if not compatibility and record.get('compatibility_only'):
            continue
        flags=['--compatibility']if compatibility else[]
        if record.get('simulate'):
            flags.append('--simulate')
        result=subprocess.run([str(compiler),'--emit-jedec',str(fixtures/(name+'.PDS')),'--settings',str(fixtures/(name+'.MXL')),*flags],capture_output=True,timeout=60)
        if (not compatibility and record.get('normal_error')) or (compatibility and record.get('compatibility_error')):
            assert result.returncode and not result.stdout and result.stderr,(name,'normal rejection')
            continue
        assert result.returncode==0,(name,flags,result.stderr)
        target=record.get('reference',name+'.JED') if compatibility else record.get('normal_reference',name)+'.JED'
        reference=jedec.parse((fixtures/target).read_bytes())
        comparison=jedec.compare(reference,jedec.parse(result.stdout))
        assert comparison['equal'],(name,flags,comparison)
print(f'{len(records)} original settings policies match compatibility and native controls')
