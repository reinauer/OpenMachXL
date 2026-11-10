# Original MACHXL regression corpus

These are original DOS compiler outputs for all 33 bundled examples, using
the supplied MXL synthesis settings. They are test fixtures, never an output
lookup table for the native compiler. Source inputs remain in
`dos/EXAMPLES.ZIP`; the manifest records their hashes, original executable
hashes, archive hashes, effective setting overrides, and output hashes.

`tools/dos_oracle.py` produced these files in fresh isolated directories.
The complete run was repeated independently: all 33 outcomes matched and
all 32 successful main/electrical/auxiliary fuse maps matched. Textual dates
and run times differ. `NOT2BIG` failed in both runs with original error
`z8007` at 95 percent partitioning; its log is retained and no JEDEC exists.

PLA files provide the original ordered minimized cover. PLC files provide
original placement. DBG files record actual stage commands and flags.
JEDEC files provide the target fuse maps and configuration records.
Passing the fixture integrity test proves these files are unchanged and
well-formed; it does not establish native compiler compatibility.
