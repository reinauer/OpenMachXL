#!/usr/bin/env python3
"""Actual failed partition observations, with explicit original comparison limits."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,tempfile
from report_banner import without_banner
root=Path(__file__).with_name('partition-failure-reports')
manifest=json.loads((root/'manifest.json').read_text())
for name,digest in manifest['sha256'].items():
 assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest,name
original=(root/'NOT2BIG.RPT').read_bytes()
def summary(data):
 rows=re.findall(rb'^Block ([A-H]) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +([0-9]+) +\.\.',data,re.M)
 return {b.decode():list(map(int,values)) for b,*values in rows}
def remaining(data):
 part=data.split(b'( excluding pins used only as inputs )\r\n')[1].split(b'\r\n\r\n')[0]
 return part.split()
def failure(data):
 return data.split(b"Signal '")[1].split(b'\r\n\r\n')[0]
def membership(data):
 return sorted(re.findall(rb'^([A-H])\? +\.\. +(\S+) ',data,re.M))
def free_clusters(data):
 return re.findall(rb'^Block [A-H] .+? +(\d+)\r?$',data,re.M)
def comparable_report(data):
 # Keep every signal row and its original ordering. Only the proven DOS
 # global-summary accumulator bug and invocation metadata differ.
 data=without_banner(data)
 data=re.sub(rb'^Logic Macrocells[^\r\n]*\r\n',
             b'Logic Macrocells                128       68     60    -->    53%\r\n',data,flags=re.M)
 data=re.sub(rb'^\* Design Name = [^\r\n]*\r\n',b'* Design Name = <name and time> *\r\n',data,flags=re.M)
 return re.sub(rb'^(Start:|End  :)[^\r\n]*\r\n',rb'\1 <time>\r\n',data,flags=re.M)
old_rows=summary(original)
assert sum(row[4]for row in old_rows.values())==68
with tempfile.TemporaryDirectory(prefix='machxl-partition-report-')as work:
 destination=Path(work)/'reports'
 for compatibility in (False,True):
  command=[sys.argv[1],'--emit-jedec',str(root/'NOT2BIG.PDS'),'--settings',str(root/'NOT2BIG.MXL')]
  if compatibility:command.append('--compatibility')
  plain=subprocess.run(command,capture_output=True,timeout=30)
  assert plain.returncode and not plain.stdout
  destination.mkdir(exist_ok=True)
  for suffix in ['RPT','PRD','TAL','HST','TRF']:(destination/('NOT2BIG.'+suffix)).write_text('stale')
  (destination/'UNRELATED.RPT').write_text('keep')
  result=subprocess.run(command+['--fitter-reports',str(destination),'--simulate','--simulation-reports',str(destination)],capture_output=True,timeout=30)
  assert result.returncode and not result.stdout and result.stderr==plain.stderr
  assert sorted(p.name for p in destination.iterdir())==['NOT2BIG.RPT','NOT2BIG.TAL','UNRELATED.RPT']
  actual=(destination/'NOT2BIG.RPT').read_bytes();rows=summary(actual)
  assert failure(actual)==failure(original)
  assert remaining(actual)==remaining(original)==[b'RESESIGJNT',b'SIGBE',b'SIGBH',b'SIGBK',b'SIGBN']
  assert rows==old_rows
  assert membership(actual)==membership(original)
  assert free_clusters(actual)==free_clusters(original)
  assert comparable_report(actual)==comparable_report(original)
  assert sum(row[4]for row in rows.values())==68
  assert sum(row[1]for row in rows.values())==sum(row[1]for row in old_rows.values())==105
  assert sum(row[2]for row in rows.values())==sum(row[2]for row in old_rows.values())==55
  assert b'Logic Macrocells                128       68     60' in actual
  assert b'Product Term Clusters           128       53     75' in actual
  assert b'Partitioning  95% - Completed\r\nPlacement      0% - Completed\r\nRouting        0% - Completed' in actual
  assert actual.endswith(b'%%% Fitting process has failed %%%\r\n')
  assert len(re.findall(rb'^[A-H]\? +\.\. ',actual,re.M))==68
  assert b'mxA?' in actual and b'mcell ?-?' in actual
  # TAL preserves the caller's source filename; DOS title-cases its name.
  expected_timing=(root/'NOT2BIG.TAL').read_bytes().replace(b'Not2big.pds',b'NOT2BIG.PDS')
  assert (destination/'NOT2BIG.TAL').read_bytes()==expected_timing
 # Rename a failing logical signal to prove diagnostics come from this
 # invocation's graph rather than a retained name or fixture lookup.
 source=Path(work)/'RENAMED.PDS';source.write_bytes(re.sub(rb'\bsigbe\b',b'native_fail',(root/'NOT2BIG.PDS').read_bytes(),flags=re.I))
 result=subprocess.run([sys.argv[1],'--emit-jedec',str(source),'--settings',str(root/'NOT2BIG.MXL'),'--fitter-reports',str(destination)],capture_output=True,timeout=30)
 assert result.returncode and not result.stdout
 changed=(destination/'RENAMED.RPT').read_bytes()
 assert b"Signal 'NATIVE_FAIL' cannot be placed" in changed and b'SIGBE' not in changed
print('Failed partition reports: complete original RPT including signal order (except confirmed DOS global macrocell count bug), source-derived rename, and lifecycle passed in both modes')
