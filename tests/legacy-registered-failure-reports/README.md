Untouched MACHXL 2.1 R11 registered and input-paired failure snapshots. The
manifest records source/settings/report SHA-256 hashes and capture provenance.
Reproduce with `research/reports/capture_legacy_registered_failures.py` and
preserve results with `research/reports/promote_legacy_registered_failures.py`.

FR111/FR120 fail output-pin assignment with registered outputs. RR111,
RR211 and RR215 fail routing with a registered output and a dedicated clock.
IR211/IR215 pair a fixed raw input pad with an input register, consume both
raw and registered data, and exhaust physical routing.

The tests require every RPT/PRD/TAL byte to match after filename and execution
timestamp normalization. No table, counter, warning, clock, route or resource
content is excluded. Both modes also require an unchanged failing exit status
and diagnostic, no partial JEDEC, stale-report cleanup and unrelated-file
preservation. The native executable never reads these fixtures.
