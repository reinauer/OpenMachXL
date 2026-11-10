#!/usr/bin/env python3
"""Original PRD physical tables, allowing only documented dynamic fields."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from report_banner import without_banner

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import jedec

def normalized(data):
    # No whitespace/table normalization: DOS CRLF and every field survive.
    data=without_banner(data)
    data=re.sub(rb'^Start: .*\r\n',b'Start: <time>\r\n',data,flags=re.M)
    data=re.sub(rb'^End  : .*\r\n',b'End  : <time>\r\n',data,flags=re.M)
    return data

def main():
    corpus=ROOT/'tests/fitter-place-reports-corpus'
    manifest=json.loads((corpus/'manifest.json').read_text())
    for case in manifest['cases']:
        name=case['name'];folder=corpus/name
        for filename,digest in case['sha256'].items():
            assert hashlib.sha256((folder/filename).read_bytes()).hexdigest()==digest,filename
        original=(folder/(name+'.PRD')).read_bytes()
        display=re.search(rb'Design \[(.*?)\]',original)[1].decode()
        stamp=re.search(rb'^Start: ([^\r\n]+)',original,re.M)[1].decode()
        process=subprocess.run([sys.argv[1],str(folder/(name+'.PDS')),str(folder/(name+'.MXL')),display,stamp],capture_output=True)
        assert not process.returncode,(name,process.stderr)
        assert case['place_counter'] and case['route_counter'],(name,'counter comparison must be enabled')
        expected=normalized(original);actual=normalized(process.stdout)
        assert actual==expected,(name,'place/route report differs')
        if case.get('check_jedec') and len(sys.argv)>2:
            expected_image=jedec.parse((folder/(name+'.JED')).read_bytes())
            for flags in ([],['--compatibility']):
                image=subprocess.run([sys.argv[2],*flags,'--emit-jedec',str(folder/(name+'.PDS')),'--settings',str(folder/(name+'.MXL'))],capture_output=True,timeout=60)
                assert not image.returncode,(name,image.stderr)
                actual_image=jedec.parse(image.stdout)
                assert jedec.compare(expected_image,actual_image)['equal'] and actual_image.transmission_verified,(name,'complete JEDEC differs')
    if len(sys.argv)>2:
        with tempfile.TemporaryDirectory() as output:
            folder=corpus/'DECODE'
            process=subprocess.run([sys.argv[2],'--compatibility','--emit-jedec',str(folder/'DECODE.PDS'),'--settings',str(folder/'DECODE.MXL'),'--fitter-reports',output],capture_output=True)
            assert not process.returncode,process.stderr
            report=(Path(output)/'DECODE.PRD').read_bytes()
            assert re.search(rb'^Start: [A-Z][a-z]{2} [A-Z][a-z]{2} +\d+ \d\d:\d\d:\d\d \d{4}\r$',report,re.M),'ctime report timestamp'
            assert b'Design [decode.pds]\r\n' in report,'CLI report source name'
            expected=(folder/'DECODE.PRD').read_bytes()
            expected=re.sub(rb'Design \[.*?\]',b'Design [decode.pds]',expected)
            assert normalized(report)==normalized(expected),'CLI tables or counters differ'
    print(f'{len(manifest["cases"])} exact PRD tables, search markers and both attempt counters; only timestamps normalized')

if __name__=='__main__':main()
