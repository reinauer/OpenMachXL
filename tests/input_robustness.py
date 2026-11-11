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

SIMULATED_INPUT_REGISTER = ('CHIP T MACH435\nPIN 8 /A IPAIR N\nNODE ? N REG\nPIN 20 CLK\nPIN 9 Q\n'
                            'EQUATIONS\nN=A\nN.CLKF=CLK\nQ=N\n'
                            'SIMULATION\nSETF /A /CLK\nSETF A\nCLOCKF CLK\n')
SAVED_SIMULATION_PLACEMENT = '[VERSION]=MACHXL 2.0\n[SIM_PLACE]=Saved placement\n'

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
    # A saved simulation placement names a node location beyond the device.
    # The number used to size and index the node table without a check.
    *[dict(name='saved_simulation_node_' + label,
           files={'T.PDS': SIMULATED_INPUT_REGISTER, 'T.MXL': SAVED_SIMULATION_PLACEMENT,
                  'T.BLC': 'Pin 8 /A ; IPair 135\nPin 9 Q Comb ; S6=1 S9=1 Pair 2\nPin 20 CLK\n'
                           'Node ' + number + ' N Reg ;\n'},
           args=['--emit-jedec', 'T.PDS', '--settings', 'T.MXL', '--simulate'], ok=False,
           message='Invalid simulation node placement for N')
      for label, number in (('wraps_to_zero', '4294967295'), ('negative', '-1'), ('past_device', '200'))],
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
