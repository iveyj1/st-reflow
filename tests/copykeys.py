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
    elif len(sys.argv) == 1:
        keys = re.findall(r'"([^"]+)"', table())
        readme = (ROOT / 'README.md').read_text().split('## Keyboard copy mode', 1)[1].split('\n## ', 1)[0]
        readme = '\n'.join(line for line in readme.splitlines() if line.startswith('|'))
        docs = [('README copy-mode table', readme)]
        dwm = ROOT.parent / 'dwm'
        if dwm.exists():
            text = (dwm / 'dwm-keymap').read_text()
            docs.append(('dwm-keymap', '\n'.join(line for line in text.splitlines() if 'Copy mode:' in line)))
        else:
            print('SKIP: sibling dwm repository not present')
        missing = []
        for name, text in docs:
            for key in keys:
                if not re.search(r'(?<![A-Za-z0-9+])' + re.escape(key) + r'(?![A-Za-z0-9+])', text):
                    missing.append(f'{name}: {key}')
        if missing:
            raise SystemExit('Undocumented copy keys:\n' + '\n'.join(missing))
        print(f'All {len(keys)} copy-key bindings documented')
    else:
        raise SystemExit('usage: copykeys.py [--emit]')
