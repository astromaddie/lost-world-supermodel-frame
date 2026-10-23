#!/usr/bin/env python3
"""Reject ELF version requirements newer than the Steam Frame ABI floor."""

import re
import sys


ABI_LIMITS = {
    "GLIBC": (2, 35),
    "GLIBCXX": (3, 4, 30),
}


def versions_over_limit(readelf_output):
    violations = set()
    for name, raw_version in re.findall(r"Name: (GLIBCXX|GLIBC)_([0-9]+(?:\.[0-9]+)*)", readelf_output):
        version = tuple(int(part) for part in raw_version.split("."))
        limit = ABI_LIMITS[name]
        width = max(len(version), len(limit))
        padded_version = version + (0,) * (width - len(version))
        padded_limit = limit + (0,) * (width - len(limit))
        if padded_version > padded_limit:
            violations.add(f"{name}_{raw_version}")
    return sorted(violations)


def main():
    violations = versions_over_limit(sys.stdin.read())
    if violations:
        print("\n".join(violations), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
