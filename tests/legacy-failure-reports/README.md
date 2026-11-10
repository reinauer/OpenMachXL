Untouched MACHXL 2.1 R11 failure reports and exact compiler inputs.
`manifest.json` records source/settings/output SHA-256 hashes and the original
reference settings overrides. Reproduce with
`python3 research/reports/capture_legacy_failures.py --output build/recapture`.
The capture uses archived DOS tools; the native compiler never reads this corpus.

R110 through R231 cover twelve MACH1xx/2xx device types. Fixed raw inputs have
insufficient physical CSM connectivity despite fitting the logical fan-in
limit. The tests compare complete RPT, PRD and TAL reports, including retained
routes, unrouteable-signal markers and attempt counters.

E110/E111/E120/E131 put three distinct OE controls on fixed pads in one half
with two OE resources. DOS fails during partitioning and emits RPT/TAL only.
Native now retains the same partial partition. The sole report correction is
DOS's doubled product-group count labeled as Logic Macrocells; native reports
two retained macrocells rather than four. See
`research/fitter/FAILED_PARTITION_REPORTS.md` for report details.

F110/F111/F120/F131 reserve all but three block-A pads as inputs, with the
three free pads in one half. Three floating outputs require distinct OEs.
The partition fits, but physical cell-to-pad assignment exhausts its search.
The original final cell assignment, product allocation, search counters and
report tables are compared. The raw `OErc[index:count]` warning is derived
from the archived DEV selector tables and the actual last OE lookup; both
it and the descriptive failed pad-assignment error are compared exactly.

Tests run both compiler modes with and without reports, require unchanged
failure diagnostics and empty JEDEC output, remove stale simulation/success
reports, and preserve unrelated files. Only timestamps and caller filename
case are normalized in the other report comparisons.
