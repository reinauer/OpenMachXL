#!/usr/bin/env python3
"""Native-only checks of original MACHPAR context-specific reserved names."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from compare_pla import parse_pla, read_pla, compare_pla
from jedec import parse as parse_jedec, read as read_jedec, compare as compare_jedec


def locations(data):
    return sorted((kind, signal) for kind, fields in
                  re.findall(r'^#AMD (PIN|NODE) \d+ (.*)', data.decode('latin1'), re.M)
                  for signal in fields.split())


def main():
    executable = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parent / 'frontend-reserved'
    manifest = json.loads((root / 'manifest.json').read_text())
    for case in manifest['cases']:
        source = root / (case['name'] + '.PDS')
        for extension, digest in case['sha256'].items():
            path = root / (case['name'] + '.' + extension)
            assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, path
        original_log=source.with_suffix('.LOG').read_text(encoding='latin1')
        assert ('|> ERROR ' in original_log)==case['rejected']
        if not case['rejected']:
            assert 'File processed successfully.' in original_log
        for mode in [[], ['--compatibility']]:
            command = [str(executable), *mode]
            if 'MXL' in case['sha256']:
                command += ['--settings', str(source.with_suffix('.MXL'))]
            result = subprocess.run([*command, '--emit-pla', str(source)],
                                    capture_output=True, timeout=10)
            if case['rejected']:
                assert result.returncode and not result.stdout, (case.get('context','CHIP'),case['keyword'],mode)
            else:
                assert not result.returncode and b'.e\n' in result.stdout, (case.get('context','CHIP'),case['keyword'],mode,result.stderr)
                if 'PLA' in case['sha256']:
                    reference = source.with_suffix('.PLA')
                    assert compare_pla(parse_pla(result.stdout), read_pla(reference))['matrix_equal'], (case['name'], mode)
                    assert locations(result.stdout) == locations(reference.read_bytes()), (case['name'], mode)
                if 'JED' in case['sha256']:
                    fitted = subprocess.run([*command, '--emit-jedec', str(source)],
                                            capture_output=True, timeout=20)
                    assert not fitted.returncode, (case['name'], mode, fitted.stderr)
                    assert compare_jedec(parse_jedec(fitted.stdout), read_jedec(source.with_suffix('.JED')))['equal'], (case['name'], mode)
    accepted=sum(not case['rejected'] for case in manifest['cases'])
    print(f"{len(manifest['cases'])} original reserved-name outcomes matched in both modes ({accepted} accepted)")


if __name__ == '__main__':
    main()
