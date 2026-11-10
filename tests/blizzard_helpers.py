#!/usr/bin/env python3
"""Check original acceptance/rejection and complete helper compilation output."""
from pathlib import Path
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from blizzard_reference import prepare_plcc
from compare_pla import parse_pla,compare_pla
import jedec
compiler=Path(sys.argv[1]).resolve();fixtures=ROOT/'tests/blizzard-helper-reference'
records=json.loads((fixtures/'manifest.json').read_text())['cases']
for r in records:
    name=r['name']
    for filename,digest in r['files'].items():
        assert hashlib.sha256((fixtures/filename).read_bytes()).hexdigest()==digest,filename
    assert prepare_plcc((fixtures/(name+'.tqfp.pds')).read_bytes())==(fixtures/(name+'.PDS')).read_bytes(),name
    for flags in ([],['--compatibility']):
        # A complete fit necessarily runs the rejected frontend stage;
        # avoid repeating expensive invalid-cover expansion just to fail twice.
        for mode in (('pla','jedec')if r['frontend_success']else('jedec',)):
            p=subprocess.run([str(compiler),'--emit-'+mode,str(fixtures/(name+'.PDS')),'--settings',str(fixtures/(name+'.MXL')),*flags],capture_output=True,timeout=60)
            expected=r['frontend_success']if mode=='pla'else r['success']
            if not expected:
                assert p.returncode and not p.stdout and p.stderr,(name,flags,mode,'expected original rejection')
                continue
            assert p.returncode==0,(name,flags,mode,p.stderr)
            if mode=='pla':
                comparison=compare_pla(parse_pla((fixtures/(name+'.PLA')).read_bytes()),parse_pla(p.stdout))
                assert comparison['matrix_equal'],(name,flags,comparison)
            else:
                comparison=jedec.compare(jedec.parse((fixtures/(name+'.JED')).read_bytes()),jedec.parse(p.stdout))
                assert comparison['equal'],(name,flags,comparison)
print(f'{len(records)} Blizzard helper original outcomes match in both modes')
