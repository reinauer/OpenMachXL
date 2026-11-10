"""The banner at the top of a report names the program that wrote it.

The reference reports come from the original compiler and carry its banner;
the reports written here carry this project's. The comparisons are about the
report, so both banners are replaced by the same placeholder.
"""
import re

_BANNER = re.compile(rb'^[^\r\n]*\b(MACHFITR|MACHSIMU)\b[^\r\n]*\r?\n [^\r\n]*(?:COPYRIGHT|Copyright)[^\r\n]*', re.M)


def without_banner(report):
    """`report` (bytes) with its first program banner replaced by a placeholder."""
    return _BANNER.sub(lambda match: b'<' + match[1] + b' banner>', report, count=1)
