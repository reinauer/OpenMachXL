#!/usr/bin/env python3
"""Check fresh original-compiler language probes, including exact row order."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compare_pla import compare_pla, read_pla
from frontend_corpus import compare, alias
from jedec import read as read_jedec, compare as compare_jedec
import re


def main():
    compiler = Path(sys.argv[1]).resolve()
    fixtures = ROOT / 'tests/frontend-probes'
    manifest = json.loads((fixtures / 'manifest.json').read_text())
    with tempfile.TemporaryDirectory() as tmp:
        for case in manifest['cases']:
            name = case['name']
            for ext in ['PDS', 'PLA'] + (['BPP'] if 'bpp_sha256' in case else []):
                assert hashlib.sha256((fixtures / (name + '.' + ext)).read_bytes()).hexdigest() == case[ext.lower() + '_sha256']
            command=[str(compiler), '--compatibility', '--emit-pla', str(fixtures / (name + '.PDS'))]
            if 'mxl_sha256' in case:
                mxl=fixtures / (name + '.MXL')
                assert hashlib.sha256(mxl.read_bytes()).hexdigest()==case['mxl_sha256']
                command+=['--settings',str(mxl)]
            proc = subprocess.run(command,
                                  capture_output=True, check=True)
            if case.get('expected_warning'):
                assert case['expected_warning'].encode() in proc.stderr,(name,'missing compatibility warning')
            output = Path(tmp) / (name + '.PLA')
            output.write_bytes(proc.stdout)
            original=fixtures / (name + '.PLA')
            comparison = compare_pla(read_pla(output), read_pla(original))
            semantic=compare(read_pla(output),read_pla(original))
            assert semantic['equivalent'], (name,semantic)
            # No probe is exempt: all match the original row for row.
            assert comparison['matrix_equal'], (name, comparison)
            if case.get('corrected_error'):
                corrected=subprocess.run([arg for arg in command if arg!='--compatibility'],capture_output=True)
                assert corrected.returncode and case['corrected_error'].encode() in corrected.stderr,(name,'missing corrected-mode semantic rejection')
            if 'default_control_mxl_sha256' in case:
                control=fixtures/(name+'.default.MXL')
                assert hashlib.sha256(control.read_bytes()).hexdigest()==case['default_control_mxl_sha256']
            if 'corrected_pla_sha256' in case:
                corrected=fixtures / (name + '.default.PLA')
                assert hashlib.sha256(corrected.read_bytes()).hexdigest()==case['corrected_pla_sha256']
                if 'default_control_pds_sha256' in case:
                    control=fixtures / (name + '.default.PDS')
                    assert hashlib.sha256(control.read_bytes()).hexdigest()==case['default_control_pds_sha256']
                default_command=[arg for arg in command if arg!='--compatibility']
                default_proc=subprocess.run(default_command,capture_output=True,check=True)
                default_output=Path(tmp) / (name + '.default.PLA')
                default_output.write_bytes(default_proc.stdout)
                default_result=compare_pla(read_pla(default_output),read_pla(corrected))
                assert default_result['matrix_equal'],(name,'corrected default',default_result)
            if case.get('require_native_jedec'):
                reference=fixtures/(name+'.JED')
                assert hashlib.sha256(reference.read_bytes()).hexdigest()==case['jed_sha256']
                fitted=subprocess.run(['--emit-jedec' if arg=='--emit-pla' else arg for arg in command],capture_output=True,check=True)
                fitted_path=Path(tmp)/(name+'.JED');fitted_path.write_bytes(fitted.stdout)
                comparison_jedec=compare_jedec(read_jedec(reference),read_jedec(fitted_path))
                assert comparison_jedec['equal'] and all(comparison_jedec['transmission_verified']),(name,'native fitted JEDEC',comparison_jedec)
            if 'original_fit_error' in case:
                log=fixtures/(name+'.LOG')
                assert hashlib.sha256(log.read_bytes()).hexdigest()==case['log_sha256']
                assert case['original_fit_error'] in log.read_text(encoding='latin1')
                fitted=subprocess.run(['--emit-jedec' if arg=='--emit-pla' else arg for arg in command],capture_output=True)
                assert fitted.returncode != 0 and not fitted.stdout,(name,'original fitting rejection unexpectedly accepted or emitted output')
            if 'corrected_jed_sha256' in case:
                reference=fixtures/(name+'.default.JED')
                assert hashlib.sha256(reference.read_bytes()).hexdigest()==case['corrected_jed_sha256']
                default_fit=[('--emit-jedec' if arg=='--emit-pla' else arg) for arg in command if arg!='--compatibility']
                fitted=subprocess.run(default_fit,capture_output=True,check=True)
                fitted_path=Path(tmp)/(name+'.default.JED');fitted_path.write_bytes(fitted.stdout)
                comparison_jedec=compare_jedec(read_jedec(reference),read_jedec(fitted_path))
                assert comparison_jedec['equal'] and all(comparison_jedec['transmission_verified']),(name,'corrected fitted JEDEC',comparison_jedec)
            if case.get('check_pairs'):
                def pairs(path):
                    found=[]
                    for line in path.read_text(encoding='latin1').splitlines():
                        match=re.match(r'#AMD (PIN|NODE) \d+ (.*)',line)
                        if not match:continue
                        for symbol in match[2].split():
                            if '/' in symbol:
                                head,partner=symbol.split('/')
                                found.append((match[1],alias(head.split(':')[0]),alias(partner)))
                    return sorted(found)
                assert pairs(output)==pairs(original),(name,pairs(output),pairs(original))
            if case.get('check_supplies'):
                def supplies(path):
                    return re.findall(r'(?:GND|VCC):\d+',path.read_text(encoding='latin1'))
                assert supplies(output)==supplies(original),(name,'package supply pins')
            if case.get('check_pin_locations'):
                def locations(path):
                    return sorted((kind,symbol) for kind,text in re.findall(r'^#AMD (PIN|NODE) \d+ (.*)',path.read_text(encoding='latin1'),re.M) for symbol in text.split())
                assert locations(output)==locations(original),(name,'physical pairing constraints',locations(output),locations(original))
            print(name + ': exact labeled matrix', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
