# Retained failed physical searches

These four unmodified MACHXL 2.1 R11 captures test failed physical placement
reports in both native compiler modes. `manifest.json` guards every original
source, effective settings and report file with SHA-256.

| Design | Contrast | Original/native Place | Route |
| --- | --- | ---: | ---: |
| MRW4 | Fixed feedback cells obstruct raw IPAIR source | 9 | 0 |
| RPM815 | Full block, spread placement | 268 | 0 |
| FI0 | RPM815 paired pad moved from pin8 to pin7 | 642 | 0 |
| FI1 | RPM815 with spread placement disabled | 862 | 0 |

The regression compares every final non-CSM physical table and both search
counters. Execution timestamps and the source display path are normalized.
Original DOS summaries and CSM tables disagree about route completion; native
reports deliberately show only independently feasible assignments, leaving the
raw input unrouted. The test checks that correction explicitly rather than
normalizing or ignoring the native CSM table.

Report generation must preserve the original failure diagnostic and empty
programming output. Stale reports for that source are removed, unrelated files
remain, and simulation files are not emitted after fitting fails.

FI0/FI1 capture script: `research/fitter/probe_failed_imx_search.py`.
