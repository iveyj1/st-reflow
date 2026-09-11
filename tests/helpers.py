#!/usr/bin/env python3
"""Headless helper tests; mock menu, clipboard and browser commands."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Helpers(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.env = dict(os.environ, PATH=str(self.base), OUT=str(self.base / 'out'),
                        MENU_INPUT=str(self.base / 'menu-input'))
        # Isolate PATH so host clipboard/browser commands can never run.
        import shutil
        for name in ('awk', 'grep', 'sed'):
            (self.base / name).symlink_to(shutil.which(name))
        self.mock('dmenu', 'input=$(while IFS= read -r line; do printf "%s\\n" "$line"; done)\n'
                  'printf "%s" "$input" > "$MENU_INPUT"\n'
                  '[ "${CANCEL:-0}" = 0 ] || exit 1\n'
                  'printf "%s" "${CHOICE:-$input}"')
        self.mock('xclip', 'printf "%s\\n" "$*" > "$OUT.args"\n'
                  'while IFS= read -r line || [ -n "$line" ]; do printf "%s" "$line"; done > "$OUT"')
        self.mock('xdg-open', 'printf "%s" "$1" > "$OUT"')

    def mock(self, name, body):
        file = self.base / name
        file.write_text('#!/bin/sh\n' + body + '\n')
        file.chmod(0o755)

    def run_helper(self, helper, data, *args, **env):
        return subprocess.run(['/bin/sh', str(ROOT / helper), *args], input=data,
                              text=True, capture_output=True, timeout=5,
                              env=dict(self.env, **env))

    def test_copy_selected_line(self):
        result = self.run_helper('st-copyout', 'first\n\n  selected line\nlast\n',
                                 CHOICE='  selected line')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), '  selected line')
        self.assertEqual((self.base / 'menu-input').read_text(), 'first\n  selected line\nlast')

    def test_url_selection(self):
        result = self.run_helper('st-urlhandler',
                                 'https://example.org/a. https://example.org/a\nhttp://other.test\n',
                                 '-c', CHOICE='https://example.org/a')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), 'https://example.org/a')
        self.assertEqual((self.base / 'menu-input').read_text(),
                         'https://example.org/a\nhttp://other.test')
        self.assertEqual((self.base / 'out.args').read_text().strip(), '-selection clipboard')

    def test_open(self):
        result = self.run_helper('st-urlhandler', 'https://example.org/?a=1&b=2\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), 'https://example.org/?a=1&b=2')

    def test_xsel(self):
        (self.base / 'xclip').rename(self.base / 'xsel')
        result = self.run_helper('st-copyout', 'hello\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out.args').read_text().strip(), '-ib')

    def test_missing_clipboard(self):
        (self.base / 'xclip').unlink()
        result = self.run_helper('st-copyout', 'hello\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('install xclip or xsel', result.stderr)

    def test_no_selection(self):
        for helper, data in [('st-copyout', '\n'), ('st-urlhandler', 'no URLs'),
                             ('st-copyout', 'hello'), ('st-urlhandler', 'https://example.org')]:
            result = self.run_helper(helper, data, CANCEL='1')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse((self.base / 'out').exists())

    def test_reject_other_scheme(self):
        result = self.run_helper('st-urlhandler', 'https://example.org', CHOICE='file:///etc/passwd')
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.base / 'out').exists())

    def test_bad_option(self):
        self.assertEqual(self.run_helper('st-urlhandler', '', '-z').returncode, 2)


if __name__ == '__main__':
    unittest.main()
