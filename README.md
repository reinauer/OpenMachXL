# OpenMACHXL

OpenMACHXL is an open-source compiler for AMD MACH programmable logic devices
on macOS and Linux. It compiles MachXL `.pds` designs into JEDEC programming
files, with logic minimization, placement, routing, and simulation.

## Supported devices

| Family | Devices | Pins |
| --- | --- | ---: |
| MACH1 | MACH110, MACH111 | 44 |
| MACH1 | MACH120, MACH121 | 68 |
| MACH1 | MACH130, MACH131 | 84 |
| MACH2 | MACH210, MACH211, MACH215 | 44 |
| MACH2 | MACH220, MACH221 | 68 |
| MACH2 | MACH231 | 84 |
| MACH3 | MACH355 | 144 |
| MACH4 | MACH435 | 84 |
| MACH4 | MACH445, MACH446 | 100 |
| MACH4 | MACH465 | 208 |

Select the device in the source file, for example `CHIP example MACH111`.
Use the device names shown above, without package or speed-grade suffixes.
Pin assignments must use the compiler's device pin numbering; package variants
are not automatically remapped.

The compiler produces `.jed` files. Programming the chip requires a separate
programmer that supports your device.

## Build

Requirements:

- A C99 and C++17 compiler, such as Clang or GCC.
- CMake 3.16 or newer and Make, Ninja, or another CMake build tool.
- Python 3.10 or newer to run the complete regression suite.

On macOS, install the Xcode Command Line Tools and CMake. On Linux, install
your distribution's C/C++ development tools and CMake. Espresso is included
in the source tree and does not need to be installed separately.

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
```

To build only the compiler, add `-DBUILD_TESTING=OFF` to the CMake command.
Python is not required for compilation or normal use.

### Install

Install into a directory owned by your user:

```sh
cmake --install build --prefix "$HOME/.local"
```

The executable is installed as `$HOME/.local/bin/machxl`. Add that directory
to your `PATH` to invoke it as `machxl`.

For a Makefile build, `make install` is also available:

```sh
cmake -S . -B build/make -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
make -C build/make -j4
make -C build/make install
```

The default installation prefix is `/usr/local`. `CMAKE_INSTALL_BINDIR`
controls the executable directory, and `DESTDIR` supports package staging.
The Espresso copyright notice is installed in `share/licenses/machxl`.

After installation, check the version:

```sh
machxl --version
```

## Compile a design

Save this two-input AND gate as `example.pds`:

```text
CHIP example MACH111

PIN ? A
PIN ? B
PIN ? Y COMBINATORIAL

EQUATIONS
Y = A * B
```

`?` lets the fitter assign a pin. Use pin numbers when a design must match
an existing board.

Compile it to a JEDEC file:

```sh
machxl --emit-jedec example.pds > example.jed
```

To apply a project's `.mxl` settings:

```sh
machxl --emit-jedec design.pds --settings design.mxl > design.jed
```

Without `--settings`, the compiler uses its defaults. A `.mxl` file beside
the source is not loaded automatically. Project settings control minimization,
register type and polarity, placement policies, and fitting limits.

To inspect the optimized logic without placing and routing it:

```sh
machxl --emit-pla design.pds --settings design.mxl > design.pla
```

PLA and JEDEC output go to standard output; warnings and errors go to standard
error. A compilation error returns a nonzero status and produces no partial
PLA or JEDEC data. Shell redirection still creates or truncates the destination
file, so check the command's exit status before using it.

### Command-line options

Choose one output mode, `--emit-pla` or `--emit-jedec`, and provide one input
file. Options may appear before or after the input filename.

| Option | Purpose |
| --- | --- |
| `--emit-pla` | Write the minimized logic as a PLA file. |
| `--emit-jedec` | Compile, place, and route the design; write a JEDEC file. |
| `--settings FILE` | Read MachXL project settings from an `.mxl` file. |
| `--placement FILE` | Apply pin and node positions from a `.plc` or `.blc` file. Requires `--emit-jedec`. |
| `--compatibility` | Enable legacy MachXL 2.1 behavior, including known compiler and simulator quirks. |
| `--simulate` | Run the design's simulation commands and include test vectors in the JEDEC output. Requires `--emit-jedec`. |
| `--fitter-reports DIR` | Write resource, placement/routing, and timing reports. Requires `--emit-jedec`. |
| `--simulation-reports DIR` | Write simulation history and trace reports. Requires `--simulate`. |
| `--version` | Print the program version. Use on its own. |

Running `machxl` without arguments prints a usage summary.

### Compatibility mode

Use `--compatibility` when rebuilding an existing design that depends on
MachXL 2.1 behavior:

```sh
machxl --compatibility --emit-jedec design.pds \
  --settings design.mxl > design.jed
```

This option affects language interpretation, optimization, project settings,
and simulation. It also enables known legacy bugs that can change the design's
logic. Leave it off for new designs to use corrected behavior. When rebuilding
a legacy project, use its complete `.mxl` file as well as its source.

## Placement and routing

The fitter handles the device resources needed to turn logic equations into
a programming file:

- **Pin and node placement:** automatic placement, fixed assignments, and
  combinations of fixed and floating signals. Saved `.plc` and `.blc` files
  can supply placement constraints.
- **Logic-block allocation:** partitions equations across blocks and assigns
  macrocells, buried nodes, and product terms. Product-term steering allows
  equations to use neighboring resources where the device supports it.
- **Signal routing:** connects inputs and feedback through the central switch
  matrix, retrying alternative placements and partitions when necessary.
- **Clocks and controls:** allocates dedicated clocks, register controls,
  output enables, and shared reset/preset terms according to the device.
- **Device configuration:** generates fuse maps and checksums, including
  low-power configuration and user signatures on devices that support them.

Source declarations such as `LOW_POWER_LIST` and `MACH_SEG_A` constrain power
usage and block placement. MXL settings such as `BALANCED_PAR`, `SPREAD_PL`,
and `ITERATE_FITR` control block balancing, placement spreading, and retries.
`M3XX_RUNTIME` limits fitting time in 15-minute units; zero means no time limit.

To fit with a saved placement:

```sh
machxl --emit-jedec design.pds --settings design.mxl \
  --placement design.plc > design.jed
```

## Simulation and reports

Add `--simulate` to execute the source's `SIMULATION` section, including
`SETF`, `CLOCKF`, `CHECK`, loops, and `PRELOAD`:

```sh
machxl --emit-jedec design.pds --settings design.mxl --simulate \
  --fitter-reports build/reports \
  --simulation-reports build/reports > design.jed
```

Reports use the source filename's stem. For `design.pds`, the files are:

| File | Contents |
| --- | --- |
| `design.RPT` | Resource use and logic-block partitioning. |
| `design.PRD` | Pin/node placement and signal routing. |
| `design.TAL` | Structural timing analysis. Values count switch-matrix passes, not nanoseconds. |
| `design.HST` | Simulation history and check results. |
| `design.TRF` | Signal traces requested by `TRACE_ON` in the source. |

Report directories are created automatically. Rerunning a design replaces its
requested reports. Failed fits can still produce reports to help diagnose
resource or routing problems.

Simulation is enabled only by `--simulate`; the MXL `SIM_OPT` preference does
not start it automatically. `SIM_PLACE` accepts `Last successful placement`
(the default, using the current fit), `Design file`, or `Saved placement`
(using the sibling `.blc` file). With `[SIMF_OPT]=Y`, simulation commands come
from the sibling `.SIM` file, with an optional `.TBL` file.

`CHECK` mismatches are reported as warnings and do not change the process exit
status. Review the diagnostics or history report. Floating simulation pins can
prevent JEDEC vector generation, and `PRELOAD` disables subsequent vectors.
For MACH3/4 devices, `Design file` simulation does not generate JEDEC vectors;
use the current fit or a saved placement instead.

## Regression tests

Build with testing enabled and run CTest:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --parallel 4
```

The tests cover language parsing, minimization, device configuration, fitting,
simulation, and report generation. The complete suite needs Python 3.10 or
newer, the data in `tests/`, and `dos/EXAMPLES.ZIP` and `dos/DAT.ZIP`. Python
packages beyond the standard library are not required.

List the configured tests with:

```sh
ctest --test-dir build -N
```

There are currently 53 tests. If only 19 appear, CMake did not find Python;
install it and rerun configuration. To select an interpreter explicitly, add
`-DPython3_EXECUTABLE=/path/to/python3` to the CMake command.

Run just the optimizer tests with:

```sh
ctest --test-dir build -R espresso_optimizer --output-on-failure
```

The full test log is saved in `build/Testing/Temporary/LastTest.log`.
