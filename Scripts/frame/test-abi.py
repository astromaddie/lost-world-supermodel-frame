#!/usr/bin/env python3
"""Regression checks for the production ABI version validator."""

import subprocess
import sys
from pathlib import Path


validator = Path(__file__).with_name("check-abi.py")
within_limits = """
Name: GLIBC_2.2
Name: GLIBC_2.35
Name: GLIBCXX_3.4
Name: GLIBCXX_3.4.30
Name: GLIBCXX_3.4.30.0
"""
too_new = """
Name: GLIBC_2.36
Name: GLIBCXX_3.4.31
"""

ok = subprocess.run([sys.executable, str(validator)], input=within_limits, text=True, capture_output=True)
assert ok.returncode == 0, ok.stderr
bad = subprocess.run([sys.executable, str(validator)], input=too_new, text=True, capture_output=True)
assert bad.returncode == 1, (bad.returncode, bad.stdout, bad.stderr)
assert "GLIBC_2.36" in bad.stderr, bad.stderr
assert "GLIBCXX_3.4.31" in bad.stderr, bad.stderr
print("Frame ABI validator tests passed.")
