#!/usr/bin/env python3
"""Extract the actual copy-mode key table for headless parser tests."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def table():
    source = (ROOT / 'config.def.h').read_text()
    match = re.search(r'static const CopyKey copykeys\[\] = \{.*?\n\};', source, re.S)
    if not match:
        raise RuntimeError('copykeys table missing')
    return match.group()

if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '--emit':
        print(table())
    else:
        raise SystemExit('usage: copykeys.py --emit')
