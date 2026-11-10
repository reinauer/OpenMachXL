"""Exact original comparison vectors and explicit corrected-source controls."""
import hashlib,json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import jedec
compiler=Path(sys.argv[1]).resolve()
fixtures=ROOT/'tests/simulation-relations-reference'
manifest=json.loads((fixtures/'manifest.json').read_text())
for case in manifest['cases']:
    name=case['name']
    for filename,digest in case['files'].items():
        assert hashlib.sha256((fixtures/filename).read_bytes()).hexdigest()==digest,filename
    for flags,suffix in [(['--compatibility'],''),([],'.default')]:
        result=subprocess.run([str(compiler),*flags,'--emit-jedec',str(fixtures/(name+'.PDS')),
            '--settings',str(fixtures/(name+'.MXL')),'--simulate'],capture_output=True,timeout=30)
        if not case['original_success']:
            assert result.returncode and not result.stdout,(name,flags,'expected original rejection')
            continue
        assert not result.returncode,(name,flags,result.stderr)
        expected=jedec.parse((fixtures/(name+suffix+'.JDC')).read_bytes())
        actual=jedec.parse(result.stdout)
        comparison=jedec.compare(expected,actual)
        assert comparison['equal'] and actual.transmission_verified,(name,flags,comparison)
    print(name+': original outcome and corrected control match')
