#!/usr/bin/env python3
"""Compare native simulation vectors with saved original MACHSIMU records."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from jedec import read as read_jedec, compare as compare_jedec

def main():
    fixtures=ROOT/'tests/simulator-corpus'
    manifest=json.loads((fixtures/'manifest.json').read_text())
    for case in manifest['cases']:
        name=case['name']
        for extension,digest in case['sha256'].items():
            assert hashlib.sha256((fixtures/(name+'.'+extension)).read_bytes()).hexdigest()==digest,(name,extension)
        original=read_jedec(fixtures/(name+'.JDC'))
        assert original.transmission_verified,(name,'original transmission checksum')
        jedec=(fixtures/(name+'.JDC')).read_text(encoding='latin1')
        expected=re.findall(r'^V\d+ ([01HLXZCNKPUD]+)\*',jedec,re.M)
        declared=re.search(r'^QV(\d+)\*',jedec,re.M)
        assert declared and int(declared[1])==len(expected),(name,'original vector count')
        command=[sys.argv[1],str(fixtures/(name+'.PDS'))]
        if case.get('original_placement'):command.append(str(fixtures/(name+'.PLC')))
        if case.get('effective_settings'):command.append(str(fixtures/(name+'.MXL')))
        process=subprocess.run(command,capture_output=True,text=True,check=True)
        lines=process.stdout.splitlines()
        assert int(lines[0])==case['check_mismatches'],(name,'CHECK mismatches',lines[0])
        assert lines[1:]==expected,(name,'JEDEC vectors',lines[1:],expected)
        if case.get('native_fit'):
            assert len(sys.argv)>2,'native compiler argument required'
            for flags in ([],['--compatibility']):
                fitted_original=original;fitted_expected=expected;fitted_clashes=case['check_mismatches']
                if not flags and case.get('corrected_reference'):
                    corrected=fixtures/(case['corrected_reference']+'.JDC')
                    fitted_original=read_jedec(corrected)
                    fitted_expected=re.findall(r'^V\d+ ([01HLXZCNKPUD]+)\*',corrected.read_text(encoding='latin1'),re.M)
                    fitted_clashes=case['corrected_check_mismatches']
                command=[sys.argv[2],*flags,'--emit-jedec',str(fixtures/(name+'.PDS')),'--simulate']
                if case.get('effective_settings'):command.extend(['--settings',str(fixtures/(name+'.MXL'))])
                native=subprocess.run(command,capture_output=True,check=True)
                actual=re.findall(r'^V\d+ ([01HLXZCNKPUD]+)\*',native.stdout.decode('ascii'),re.M)
                assert actual==fitted_expected,(name,'native fitted vectors')
                assert native.stderr.count(b'CHECK mismatch')==fitted_clashes,(name,'native fitted CHECK mismatches')
                with tempfile.TemporaryDirectory()as directory:
                    output=Path(directory)/(name+'.JED');output.write_bytes(native.stdout)
                    comparison=compare_jedec(fitted_original,read_jedec(output))
                    if not flags and case.get('compatibility_fuses_only'):
                        assert comparison['auxiliary_equal'] and all(comparison['transmission_verified']),(name,'corrected-mode simulation records',comparison)
                    else:
                        assert comparison['equal']and all(comparison['transmission_verified']),(name,'native fitted JEDEC',comparison)
        print(f'{name}: {len(expected)} exact original vectors, {lines[0]} CHECK mismatches')
    if len(sys.argv)>2:
        # -BR emits a fit even when the interactive MXL SIM_OPT is enabled.
        with tempfile.TemporaryDirectory()as directory:
            settings=Path(directory)/'SIM.MXL';settings.write_text('[SIM_OPT]=Y\n[SIMF_OPT]=Y\n')
            result=subprocess.run([sys.argv[2],'--emit-jedec',str(fixtures/'SIMBASE.PDS'),'--settings',str(settings)],capture_output=True,check=True)
            assert not re.search(rb'^QV',result.stdout,re.M),'SIM_OPT must not silently add vectors to fit-only output'
        invalid=subprocess.run([sys.argv[2],'--emit-pla',str(fixtures/'SIMBASE.PDS'),'--simulate'],capture_output=True)
        assert invalid.returncode and not invalid.stdout and invalid.stderr,'invalid simulation mode emitted output'
    return 0

if __name__=='__main__':
    raise SystemExit(main())
