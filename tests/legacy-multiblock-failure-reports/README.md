Untouched MACHXL 2.1 R11 multi-block failure snapshots. Each capture includes
source, settings, RPT/PRD/TAL and the original log, with SHA-256 provenance.
The compiler never reads these fixtures at runtime.

MF111A and MF111B force output-pin assignment to fail in opposite blocks.
MP111 and MPA force a nine-product cell allocation to fail in opposite blocks,
while another block retains its first cell placement. MR111 has a routing
failure in A and a successfully routed feedback node in B. RF112 consumes
registered Q feedback in B alongside a raw input and verifies their separate
logical record identities in RPT versus physical routing order in PRD.

The tests compare all report bytes after timestamp/filename normalization,
in both modes, with unchanged rejection diagnostics, empty JEDEC output and
stale-report cleanup. The `.corrected.RPT` files repair only the original
fixed-pad report duplication described in
`research/fitter/FIXED_PAD_FAILURE_REPORT_BUG.md`. Original captures remain
unchanged; `research/reports/promote_legacy_multiblock_failures.py` derives the
corrections explicitly. Unknown cell-to-pad links remain unknown.
