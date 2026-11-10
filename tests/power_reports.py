#!/usr/bin/env python3
"""Check power annotations and corrected ICC reference values."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from dataclasses import replace

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from jedec import compare, parse, read
from compare_pla import parse_pla

root=Path(__file__).with_name('power-report-reference')
records=json.loads((root/'manifest.json').read_text())['cases']
marker=b'***********************\r\n* MACH FITTER OPTIONS'

def tables(report):
    # Only the banner/timestamp and explicitly defective original ICC paragraph
    # precede this marker. Every options/resource/signal/legend byte is compared.
    return report[report.index(marker):]

corrected_controls=0
with tempfile.TemporaryDirectory(prefix='machxl-power-') as temp:
    for case in records:
        name=case['name'];folder=root/name
        for filename,digest in case['sha256'].items():
            assert hashlib.sha256((folder/filename).read_bytes()).hexdigest()==digest,(name,filename)
        original=read(folder/(name+'.JED'))
        expected_matrix=parse_pla((folder/(name+'.PLA')).read_bytes()) if case.get('require_matrix') else None
        estimate=float(subprocess.check_output([sys.argv[2],str(folder/(name+'.JED'))]))
        if 'corrected_control' in case:
            corrected=(folder/(name+'.corrected.RPT')).read_bytes()
            printed=int(re.search(rb'ICC.*is (\d+) milliamps',corrected)[1])
            assert int(estimate+0.5)==printed,(name,estimate,printed)
            assert compare(original,read(folder/(name+'.corrected.JED')))['equal'],name
            corrected_controls+=1
        expected=(folder/(name+'.RPT')).read_bytes()
        for annotation in case.get('required_power_annotations',[]):
            assert re.search(rb' +'+re.escape(annotation.encode())+rb'\r?$',expected,re.M),(name,annotation)
        for compatibility in (False,True):
            output=Path(temp)/name/str(compatibility)
            command=[sys.argv[1],'--emit-jedec',str(folder/(name+'.PDS')),
                     '--settings',str(folder/(name+'.MXL')),'--fitter-reports',str(output)]
            if compatibility:command.append('--compatibility')
            if expected_matrix is not None:
                pla_command=command[:];pla_command[1]='--emit-pla';del pla_command[pla_command.index('--fitter-reports'):pla_command.index('--fitter-reports')+2]
                pla_result=subprocess.run(pla_command,capture_output=True,timeout=30)
                assert pla_result.returncode==0,(name,compatibility,pla_result.stderr)
                actual_matrix=parse_pla(pla_result.stdout)
                for field in ('inputs','outputs','phase','rows'):
                    assert getattr(actual_matrix,field)==getattr(expected_matrix,field),(name,compatibility,field)
            result=subprocess.run(command,capture_output=True,timeout=30)
            assert result.returncode==0,(name,compatibility,result.stderr)
            expected_image=original
            if not compatibility and case.get('corrected_default_fuse_flips'):
                fuses=bytearray(original.fuses)
                for address in case['corrected_default_fuse_flips']:fuses[address]^=1
                expected_image=replace(original,fuses=bytes(fuses))
            comparison=compare(expected_image,parse(result.stdout))
            assert comparison['equal'],(name,compatibility,comparison)
            report=(output/(name+'.RPT')).read_bytes()
            assert tables(report)==tables(expected),(name,compatibility,'power report tables differ')
            for annotation in case.get('required_power_annotations',[]):
                assert re.search(rb' +'+re.escape(annotation.encode())+rb'\r?$',report,re.M),(name,compatibility,annotation)
            for annotation in case.get('forbidden_power_annotations',[]):
                assert not re.search(rb' +'+re.escape(annotation.encode())+rb'\r?$',report,re.M),(name,compatibility,annotation)
            if compatibility:
                assert b'Static ICC estimate unavailable in compatibility mode.' in report,name
                assert b'Estimated static ICC:' not in report,name
            else:
                actual=int(re.search(rb'Estimated static ICC: (\d+) milliamps',report)[1])
                assert actual==int(estimate+0.5),(name,actual,estimate)
                assert b'Calculated from the MACH446 coefficient model; not a measurement.' in report,name
print(f'{len(records)} MACH446 annotation/configuration cases in both modes; {corrected_controls} corrected ICC controls matched')
