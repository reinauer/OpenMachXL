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

STATE_MACHINE = ('CHIP T MACH111\nPIN ? CLK\nPIN ? X\nPIN ? S0 REG\nSTATE\nMEALY_MACHINE\nCLKF = CLK\n'
                 'SA = /S0\nSB = S0\nSA := X -> SB\nSB := X -> SA\nSIMULATION\n')

REGISTERED_PIN = ('CHIP T MACH211\nPIN ? CLK\nPIN ? A\nPIN ? E\nPIN ? Q REG\n{more}EQUATIONS\nQ=A\nQ.CLKF=CLK\n'
                  'Q.TRST=E\n{read}SIMULATION\nSETF /CLK A E\nSETF /E\nSETF Q\nCLOCKF CLK\n')

# name, files, arguments after the compiler, expected success, text that must
# occur in the diagnostic of an expected rejection, and optionally files an
# accepted run must write.
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
    # A saved placement puts two signals on one pin. The router used to report
    # "routing source occurs more than once" on MACH111.
    *[dict(name='saved_placement_two_signals_on_one_pin_' + device,
           files={'T.PDS': FLOATING_DESIGN.format(device=device), 'T.PLC': 'PIN 4 A\nPIN 4 B\n'},
           args=['--emit-jedec', 'T.PDS', '--placement', 'T.PLC'], ok=False,
           message='Duplicate physical pin assignment: pin 4 for A and B')
      for device in ('MACH111', 'MACH211', 'MACH435')],
    # A location written in the design itself is checked while parsing, with
    # the original parser's diagnostics (P127 pins, P128 nodes, P90 for a
    # numeral it cannot read). These used to reach std::stoi or the fitter.
    *[dict(name='declared_location_' + label,
           files={'T.PDS': FLOATING_DESIGN.format(device='MACH211').replace(old, new)},
           args=[mode, 'T.PDS'], ok=False, message=message)
      for mode in ('--emit-pla', '--emit-jedec')
      for label, old, new, message in (
          ('node_past_device', 'NODE ? N', 'NODE 40000 N', 'P128: invalid node number for this device or macro: 40000'),
          ('node_eleven_digits', 'NODE ? N', 'NODE 99999999999 N', 'P128: invalid node number'),
          ('pin_past_device', 'PIN ? A', 'PIN 45 A', 'P127: invalid pin number for this device or macro: 45'),
          ('pin_zero', 'PIN ? A', 'PIN 0 A', 'P127: invalid pin number for this device or macro: 0'),
          ('pin_twenty_digits', 'PIN ? A', 'PIN 99999999999999999999 A', 'P90: Unexpected number symbol'))],
    # An oversized vector index or saved input-pair number is rejected as out
    # of range, not converted.
    dict(name='vector_index_eleven_digits',
         files={'T.PDS': 'CHIP T MACH111\nPIN ? A[99999999999]\nPIN ? Q\nEQUATIONS\nQ=A[99999999999]\n'},
         args=['--emit-pla', 'T.PDS'], ok=False, message='Vector range exceeds frontend limit'),
    dict(name='saved_simulation_input_pair_overflow',
         files={'T.PDS': SIMULATED_INPUT_REGISTER, 'T.MXL': SAVED_SIMULATION_PLACEMENT,
                'T.BLC': 'Pin 8 /A ; IPair 99999999999999999999999\nPin 9 Q Comb ; S6=1 S9=1 Pair 2\n'
                         'Pin 20 CLK\nNode 135 N Reg ;\n'},
         args=['--emit-jedec', 'T.PDS', '--settings', 'T.MXL', '--simulate'], ok=False,
         message='Invalid simulation input pair node for A'),
    # A saved placement fixes a paired pad and its node in different blocks.
    # The placer used to keep the node's block and silently move the pad.
    dict(name='saved_placement_pair_in_two_blocks',
         files={'T.PDS': 'CHIP T MACH435\nPIN ? A\nPIN ? B\nPIN ? Q0\nNODE ? N0 PAIR Q0\nPIN ? Q1\n'
                         'EQUATIONS\nQ0=A\nN0=A\nQ1=N0*B\n',
                'T.PLC': 'PIN 3 Q0\nNODE 20 N0\n'},
         args=['--emit-jedec', 'T.PDS', '--placement', 'T.PLC'], ok=False,
         message='conflicting fixed pin and node'),
    # MACH2 pairs pad A with input register NN, then removes NN because
    # nothing uses it. Capturing the fitter report looked up the removed
    # register's cell, so requesting reports turned the fit into a failure.
    dict(name='reports_with_removed_input_register',
         files={'T.PDS': 'CHIP T MACH211\nPIN ? A\nPIN ? B\nPIN 11 CLK\nPIN ? Q\nNODE ? NN REG\n'
                         'EQUATIONS\nQ=A*B\nNN=A\nNN.CLKF=CLK\n'},
         args=['--emit-jedec', 'T.PDS', '--fitter-reports', 'R'], ok=True,
         outputs=['R/T.RPT', 'R/T.PRD', 'R/T.TAL']),
    # A STATE name in SETF or CLOCKF. The original expands it to the signals
    # of its encoding and then applies its ordinary rules to those signals;
    # the lookup of the name itself used to fail with "map::at".
    dict(name='simulation_setf_state_name',
         files={'T.PDS': STATE_MACHINE + 'SETF /CLK X\nSETF SA\nCLOCKF CLK\n'},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=False,
         message='D3530: Cannot SETF signal S0 whose references are taken from the register'),
    # SETF on a registered pin: the original's D3530 applies only when an
    # equation reads that pin and no paired node makes it read the pin itself.
    dict(name='simulation_setf_registered_pin_read_by_equation',
         files={'T.PDS': REGISTERED_PIN.format(more='PIN ? Y\n', read='Y=Q\n')},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=False,
         message='D3530: Cannot SETF signal Q whose references are taken from the register'),
    # SETF on an output that always drives its pin, and on a buried node.
    dict(name='simulation_setf_combinatorial_output',
         files={'T.PDS': 'CHIP T MACH211\nPIN ? A\nPIN ? Q\nEQUATIONS\nQ=A\nSIMULATION\nSETF A\nSETF Q\n'},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=False,
         message='D3165: Cannot SETF output pin/node Q.'),
    dict(name='simulation_setf_combinatorial_output_with_enable',
         files={'T.PDS': 'CHIP T MACH211\nPIN ? A\nPIN ? Q\nEQUATIONS\nQ=A\nQ.TRST=VCC\nSIMULATION\nSETF A\nSETF Q\n'},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=True),
    dict(name='simulation_setf_node',
         files={'T.PDS': 'CHIP T MACH211\nPIN ? A\nPIN ? Q\nNODE ? N\nEQUATIONS\nN=A\nQ=N\nSIMULATION\nSETF A\nSETF N\n'},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=False,
         message='D3340: Cannot SETF pin/node N.'),
    dict(name='simulation_setf_registered_pin_not_read',
         files={'T.PDS': REGISTERED_PIN.format(more='', read='')},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=True),
    dict(name='simulation_clockf_state_name',
         files={'T.PDS': STATE_MACHINE + 'SETF /CLK X\nCLOCKF SB\n'},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=False,
         message='D3290: Cannot CLOCKF pin S0. The pin is not a clock pin.'),
    dict(name='simulation_setf_complemented_state_name',
         files={'T.PDS': STATE_MACHINE + 'SETF /CLK X\nSETF /SA\nCLOCKF CLK\n'},
         args=['--emit-jedec', 'T.PDS', '--simulate'], ok=False,
         message='E1820: Illegal simulation element: SA'),
    # Exactly as many inputs as a block's switch matrix routes must still fit;
    # one more is the original DRC error z5524 (see the rejection reference).
    dict(name='equation_fanin_at_routing_capacity',
         files={'T.PDS': 'CHIP T MACH111\n' + ''.join('PIN ? I%d\n' % i for i in range(1, 27)) +
                         'PIN 2 O\nEQUATIONS\nO=' + '*'.join('I%d' % i for i in range(1, 27)) + '\n'},
         args=['--emit-jedec', 'T.PDS'], ok=True),
]

# Messages of exceptions thrown by the standard library, not by the compiler.
LIBRARY_MESSAGES = ('map::at', 'stoi', 'stoul', 'stoll', 'basic_string', 'vector')


def run(case, flags):
    with tempfile.TemporaryDirectory(prefix='machxl-robustness-') as directory:
        for filename, text in case['files'].items():
            (Path(directory) / filename).write_text(text)
        result = subprocess.run([str(compiler), *case['args'], *flags], cwd=directory,
                                capture_output=True, timeout=120)
        missing = [name for name in case.get('outputs', []) if not (Path(directory) / name).is_file()]
    label = (case['name'], flags)
    stderr = result.stderr.decode(errors='replace')
    assert result.returncode in (0, 1), (label, 'crashed', result.returncode, stderr[-2000:])
    diagnostic = stderr.strip().splitlines()[-1] if stderr.strip() else ''
    text = diagnostic.split(': ', 1)[-1]
    assert not any(text.strip() == m or text.startswith(m + ':') for m in LIBRARY_MESSAGES), \
        (label, 'bare library exception', diagnostic)
    if case['ok']:
        assert result.returncode == 0 and result.stdout, (label, 'expected success', stderr[-2000:])
        assert not missing, (label, 'missing output files', missing)
    else:
        assert result.returncode == 1 and not result.stdout, (label, 'expected rejection', stderr[-2000:])
        assert case['message'] in stderr, (label, 'unexpected diagnostic', stderr[-2000:])


for case in CASES:
    for flags in ([], ['--compatibility']):
        run(case, flags)
    print(case['name'] + ': ' + ('fits' if case['ok'] else 'rejected with a compiler diagnostic'))
