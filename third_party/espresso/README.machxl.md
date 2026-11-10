# Berkeley Espresso source provenance

The shared heuristic and sparse-cover implementation is updated from
CHIPS Alliance Espresso **v2.5.1**, commit
`d80d7bb96051b81d343bd66761f2768307b35a4b`:

https://github.com/chipsalliance/espresso/tree/v2.5.1

This is an integrated library with retained MachXL extensions, not the stock
upstream CLI. `UPSTREAM_INTEGRATION.json` records the adopted function names,
retained functions in shared files, and SHA-256 hashes of their upstream files.
The previous full source base was v2.4, commit
`4b6c8e1033d4124f7cc56450b42f51aeae6ef1a7`.

The original Berkeley distribution was independently downloaded from:
https://ptolemy.berkeley.edu/projects/embedded/pubs/downloads/espresso/espresso.tar.gz

Original archive SHA-256:
`757256c24d33f343068a67f51a57f8dfbe8af01d122a8434ef99572544cb4802`.

The distribution permission and warranty notice is preserved verbatim in
`utility/copyright.h` and copied to `COPYRIGHT`. All existing source notices
are retained. This is Berkeley source, not recovered AMD source.

## Retained features and representation

Version 2.5.1 removes APIs that this compiler uses, including exact
minimization (`minimize_exact`), cover intersection (`cv_intersect`), and
verification (`verify`). Its smaller CLI also removes phase/pair/signature
support and many configurable heuristic options. These modules and their
support routines remain as explicit extensions from the full v2.4 source.
Legacy PLA I/O helpers retain their extended interfaces, although MachXL uses
its own source frontend rather than the upstream CLI.

The option-aware `espresso` driver and `last_gasp` retain actual cost updates,
MXL policy branches, and the exact/heuristic distinction. The stock v2.5.1
driver removed its `EXECUTE`/`totals` cost updates without replacing them:
`cost` is consequently stale at both loop conditions. Keeping the working
cost updates avoids that upstream regression. The `make_sparse` driver likewise
retains cost refreshes after reduction/expansion and the `force_irredundant`
iteration option; the stock v2.5.1 version compares an unchanged zero cost.
Expansion retains the `use_random_order` policy branch. Cube setup retains
dimension validation, multiple-valued-variable counts and the no-output
sentinel used by the full library interface. Sparse minimum cover retains
root lower-bound accounting and the column-selection fallback for builds
without independent-set bounds. The `reduce` entry point also
retains get/set/reset accessors for its alternating heuristic, so independent
compilation sessions do not share state accidentally.

The project retains its **16-bit packed-word representation** as an existing
DOS-baseline choice, not a hardware requirement. `espresso_word` is `uint16_t`;
allocations, copies and full-word masks use that width. With `BPI=16`, the
`SIZE` value occupies an extra word and `LOOPCOPY` includes it. The adopted
implementations preserve this layout instead of silently mixing upstream
32-bit allocations with 16-bit data. Moving to a wider native representation
can be evaluated independently against Boolean correctness, optimization
quality, fitting success and the saved baseline.

All compiled function definitions, declarations and callbacks now have ANSI C
prototypes. The qsort callbacks use the standard `const void *` interface.
The build no longer disables Clang's deprecated non-prototype warning.
Legacy diagnostic and PLA-label formatting uses bounded C99 `snprintf` with
the actual buffer capacity, including Debug builds using SDK fortification.
The legacy full-set predicate uses a 16-bit all-ones mask, and set-family
hexadecimal input uses `SCNx16` for its actual destination objects. The
packed-set regression exercises word boundaries and serialization roundtrips.
It also checks cube metadata and a multi-output cleanup case where forced
iteration improves the one-pass cover from 11 products/41 literals to 10/36,
accepting equal or better quality and independently verifying equivalence.

## Integration and validation policy

`src/optimizer_bridge.c` and `src/optimizer.cpp` serialize the global Espresso
workspace, initialize options explicitly, verify optimized covers against
the original on-set modulo don't-cares, and free the workspace after each
call. Caller-specified variable order is retained. Original MACHMIN's
cover-to-TRE list prepend reverses the returned cover; the native wrapper
exposes that operation explicitly.

Saved original outputs remain a comparison baseline. A demonstrated upstream
bug fix or quality improvement need not preserve historical term ordering or
fuse choices, but must retain Boolean correctness and supported compiler
policies. The source update does not itself establish that a result is better;
quality changes require separate measured controls. Exact minimization and
legacy policy coverage remain part of the regression suite.
