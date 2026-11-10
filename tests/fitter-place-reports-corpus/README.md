Saved original MACHFITR PRD reports and exact compiler inputs. `manifest.json`
records hashes and original capture paths. All 60 physical-table reports,
search-mode markers and both attempt counters match. Only execution timestamps
are excluded from the byte comparison. No runtime fixture lookup is used.

RC0..3, DP0..3/DP5..7, PR215 and AC2110/AC2150 include fresh original JEDEC files; the CLI regression
compares complete programming maps in both modes. The DP controls cover deferred
fixed-input-pad conflicts, legacy device geometry and sequential placement. PR215 covers the MACH215 sync/async column,
zero-sized odd product groups and physical input-register layout.
AC2110/AC2150 cover accepted legacy paired input-register tables.
See `research/fitter/PLACE_REPORTS.md` for static counter evidence, diagnostic
captures and failed-fit report limits.
