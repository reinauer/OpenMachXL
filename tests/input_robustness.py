#!/usr/bin/env python3
"""Require clean results for inputs that once crashed or were silently misread.

These cases have no DOS reference. Each one records a defect found by review:
the compiler must either fit the design or reject it with its own diagnostic,
never crash and never print a bare library exception.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()

# name, files, arguments after the compiler, expected success, and text that
# must occur in the diagnostic of an expected rejection.
CASES = [
    # A pad feeding a MACH4 input register also clocks another register. It
    # must stay on an I/O pin and clock that register through a product term.
    dict(name='ipair_pad_clocks_other_register',
         files={'T.PDS': 'CHIP T MACH435\nPIN ? P IPAIR N\nNODE ? N REG\nPIN ? CK\nPIN ? Q REG\n'
                         'EQUATIONS\nN = P\nN.CLKF = CK\nQ = N\nQ.CLKF = P\n'},
         args=['--emit-jedec', 'T.PDS'], ok=True),
    # An input register cannot be clocked by its own pad.
    dict(name='ipair_pad_clocks_own_register',
         files={'T.PDS': 'CHIP T MACH435\nPIN ? P IPAIR N\nNODE ? N REG\nPIN ? Q\n'
                         'EQUATIONS\nN = P\nN.CLKF = P\nQ = N\n'},
         args=['--emit-jedec', 'T.PDS'], ok=False,
         message='input-register clock requires a dedicated pin'),
]

# Messages of exceptions thrown by the standard library, not by the compiler.
LIBRARY_MESSAGES = ('map::at', 'stoi', 'stoul', 'stoll', 'basic_string', 'vector')


def run(case, flags):
    with tempfile.TemporaryDirectory(prefix='machxl-robustness-') as directory:
        for filename, text in case['files'].items():
            (Path(directory) / filename).write_text(text)
        result = subprocess.run([str(compiler), *case['args'], *flags], cwd=directory,
                                capture_output=True, timeout=120)
    label = (case['name'], flags)
    stderr = result.stderr.decode(errors='replace')
    assert result.returncode in (0, 1), (label, 'crashed', result.returncode, stderr[-2000:])
    diagnostic = stderr.strip().splitlines()[-1] if stderr.strip() else ''
    text = diagnostic.split(': ', 1)[-1]
    assert not any(text.strip() == m or text.startswith(m + ':') for m in LIBRARY_MESSAGES), \
        (label, 'bare library exception', diagnostic)
    if case['ok']:
        assert result.returncode == 0 and result.stdout, (label, 'expected success', stderr[-2000:])
    else:
        assert result.returncode == 1 and not result.stdout, (label, 'expected rejection', stderr[-2000:])
        assert case['message'] in stderr, (label, 'unexpected diagnostic', stderr[-2000:])


for case in CASES:
    for flags in ([], ['--compatibility']):
        run(case, flags)
    print(case['name'] + ': ' + ('fits' if case['ok'] else 'rejected with a compiler diagnostic'))
