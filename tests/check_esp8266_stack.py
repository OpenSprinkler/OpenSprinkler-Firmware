#!/usr/bin/env python3
"""Guard do_loop's GCC-reported frame, not total stack/callee/VLA usage."""
import argparse
from pathlib import Path
import sys

FRAME_LIMIT = 330


def check_report(report, limit=FRAME_LIMIT):
    matches = []
    for line in report.splitlines():
        fields = line.split("\t")
        if fields[0].endswith(":void do_loop()"):
            if len(fields) != 3 or not fields[1].isdigit():
                raise ValueError("Malformed do_loop stack-usage entry")
            matches.append(int(fields[1]))
    if len(matches) != 1:
        raise ValueError("Expected exactly one do_loop stack-usage entry")
    size = matches[0]
    if size > limit:
        raise ValueError(f"ESP8266 do_loop frame is {size} bytes; limit is {limit}. Move scratch arrays into non-inlined helpers.")
    return size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        size = check_report(args.report.read_text())
    except (OSError, ValueError) as exc:
        print(f"Stack guard FAILED: {exc}", file=sys.stderr)
        return 1
    print(f"ESP8266 do_loop stack frame: {size} bytes (limit {FRAME_LIMIT})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
