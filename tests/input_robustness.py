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

FLOATING_DESIGN = 'CHIP T {device}\nPIN ? A\nPIN ? B\nPIN ? Y\nNODE ? N\nEQUATIONS\nN=A*B\nY=N+B\n'

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
    # Simulation blocks nest without limit in the source; the parser recursed
    # once per level and overflowed the stack.
    dict(name='simulation_nesting_too_deep',
         files={'T.PDS': 'CHIP T MACH111\nPIN ? A\nPIN ? Q\nEQUATIONS\nQ = A\nSIMULATION\n' +
                         'IF A THEN BEGIN ' * 50000 + 'END ' * 50000},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=False,
         message='Simulation: block nesting exceeds 256'),
    dict(name='simulation_nesting_at_limit',
         files={'T.PDS': 'CHIP T MACH111\nPIN ? A\nPIN ? Q\nEQUATIONS\nQ = A\nSIMULATION\nSETF A\n' +
                         'IF A THEN BEGIN ' * 256 + 'CHECK Q ' + 'END ' * 256},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=True),
    # Fixed locations outside the device. The fitters narrowed them to 16 bits
    # first, so 65540 fitted exactly like pin 4 and 40000 like no constraint.
    *[dict(name='fixed_location_' + device + '_' + label,
           files={'T.PDS': FLOATING_DESIGN.format(device=device), 'T.PLC': record + '\n'},
           args=['--emit-jedec', 'T.PDS', '--placement', 'T.PLC'], ok=message is None, message=message)
      for device in ('MACH111', 'MACH211', 'MACH435')
      for label, record, message in (
          ('valid_pin', 'PIN 4 A', None),
          ('valid_node', 'NODE 3 N', None),
          ('pin_aliases_16_bits', 'PIN 65540 A', 'Invalid pin location 65540 for A'),
          ('pin_past_device', 'PIN 40000 A', 'Invalid pin location 40000 for A'),
          ('pin_negative', 'PIN -3 Y', 'Invalid pin location 4294967293 for Y'),
          ('node_past_device', 'NODE 40000 N', 'Invalid node location 40000 for N'),
          ('node_below_first', 'NODE 1 N', 'Invalid node location 1 for N'))],
    # The same check applies to a node location written in the design itself.
    dict(name='fixed_location_declared_node_past_device',
         files={'T.PDS': FLOATING_DESIGN.format(device='MACH211').replace('NODE ? N', 'NODE 40000 N')},
         args=['--emit-jedec', 'T.PDS'], ok=False, message='Invalid node location 40000 for N'),
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
