#!/usr/bin/env python3
"""Byte-level RPT comparison using original reports and a fixed timestamp."""
from pathlib import Path
import difflib,hashlib,json,re,subprocess,sys,tempfile
from report_banner import without_banner

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from jedec import parse,compare

def main():
 root=Path(__file__).resolve().parent/'fitter-report-reference'
 manifest=json.loads((root/'manifest.json').read_text())
 for case in manifest['cases']:
  name=case['name']
  for ext,digest in case['sha256'].items():
   assert hashlib.sha256((root/(name+'.'+ext)).read_bytes()).hexdigest()==digest,(name,ext)
  expected=(root/(name+'.RPT')).read_bytes()
  timestamp=re.search(rb', ([A-Z][a-z]{2} .*? \d{4}) \*',expected)[1].decode()
  result=subprocess.run([sys.argv[1],str(root/(name+'.PDS')),str(root/(name+'.MXL')),timestamp,case['display_filename']],capture_output=True,timeout=30)
  assert not result.returncode,(name,result.stderr)
  if without_banner(result.stdout)!=without_banner(expected):
   diff=''.join(difflib.unified_diff(expected.decode('latin1').splitlines(True),result.stdout.decode('latin1').splitlines(True),fromfile='original',tofile='native'))
   raise AssertionError(name+'\n'+diff[:12000])
  if 'JED' in case['sha256']:
   assert len(sys.argv)>2,'A native compiler is required for report fixtures with JEDEC references'
   for compatibility in (False,True):
    command=[sys.argv[2],'--emit-jedec',str(root/(name+'.PDS')),'--settings',str(root/(name+'.MXL'))]
    if compatibility:command.append('--compatibility')
    fitted=subprocess.run(command,capture_output=True,timeout=30)
    assert not fitted.returncode,(name,compatibility,fitted.stderr)
    comparison=compare(parse(fitted.stdout),parse((root/(name+'.JED')).read_bytes()))
    assert comparison['equal'],(name,compatibility,comparison)
  if 'clock_column' in case:
   # The original labels clock 0 with the macrocell location. Normal mode
   # prints the clock; compatibility mode reproduces the original.
   for mode,flags in (('corrected',[]),('original',['--compatibility'])):
    with tempfile.TemporaryDirectory(prefix='machxl-clock-column-') as reports:
     fitted=subprocess.run([sys.argv[2],'--emit-jedec',str(root/(name+'.PDS')),'--settings',str(root/(name+'.MXL')),'--fitter-reports',reports,*flags],capture_output=True,timeout=30)
     assert not fitted.returncode,(name,mode,fitted.stderr)
     row=re.search(rb'(?m)^[A-H]\d+ +\d+ +Q +D +\d+ +\d+ +H +(\S+)',(Path(reports)/(name+'.RPT')).read_bytes())
     assert row and row[1].decode()==case['clock_column'][mode],(name,mode,row and row[1])
 print(f"{len(manifest['cases'])} original RPT reports matched byte-for-byte")
if __name__=='__main__':main()
