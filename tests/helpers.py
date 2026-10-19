#!/usr/bin/env python3
"""Headless helper tests; mock menu, clipboard and browser commands."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Helpers(unittest.TestCase):
    HISTORY = '$ first\nfirst output\n\n$ second\n  second output  \n$ \n'

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.env = dict(os.environ, PATH=str(self.base), OUT=str(self.base / 'out'),
                        MENU_INPUT=str(self.base / 'menu-input'), TMPDIR=str(self.base))
        # Isolate PATH so host clipboard/browser commands can never run.
        import shutil
        for name in ('awk', 'grep', 'sed', 'mktemp', 'rm', 'cat'):
            (self.base / name).symlink_to(shutil.which(name))
        self.mock('dmenu', 'input=$(while IFS= read -r line; do printf "%s\\n" "$line"; done)\n'
                  'printf "%s" "$input" > "$MENU_INPUT"\n'
                  '[ "${CANCEL:-0}" = 0 ] || exit 1\n'
                  'printf "%s" "${CHOICE-$input}"')
        self.mock('xclip', 'printf "%s\\n" "$*" > "$OUT.args"\ncat > "$OUT"')
        self.mock('xdg-open', 'printf "%s" "$1" > "$OUT"')

    def mock(self, name, body):
        file = self.base / name
        file.write_text('#!/bin/sh\n' + body + '\n')
        file.chmod(0o755)

    def run_helper(self, helper, data, *args, **env):
        result = subprocess.run(['/bin/sh', str(ROOT / helper), *args], input=data,
                                text=True, capture_output=True, timeout=5,
                                env=dict(self.env, **env))
        self.assertFalse(list(self.base.glob('st-copyout.*')), 'temporary files leaked')
        return result

    def test_copy_command_output(self):
        result = self.run_helper('st-copyout', self.HISTORY, CHOICE='[1] $ first')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), 'first output\n\n')
        self.assertEqual((self.base / 'menu-input').read_text(), '[2] $ second\n[1] $ first')
        result = self.run_helper('st-copyout', self.HISTORY, CHOICE='[2] $ second')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), '  second output  \n')

    def test_repeated_commands(self):
        data = '$ run\nold output\n$ run\nnew output\n$ \n'
        for number, output in ((1, 'old output\n'), (2, 'new output\n')):
            result = self.run_helper('st-copyout', data, CHOICE=f'[{number}] $ run')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.base / 'out').read_text(), output)

    def test_preserve_clipboard(self):
        for choice, cancel in (('[1] $ first', '1'), ('', '0'),
                               ('[1] altered', '0'), ('../../out', '0')):
            (self.base / 'out').write_text('previous clipboard')
            result = self.run_helper('st-copyout', self.HISTORY, CHOICE=choice, CANCEL=cancel)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.base / 'out').read_text(), 'previous clipboard')
        for data in ('', '\n', 'ordinary output\n', '$ true\n$ \n', '$ running\npartial output\n'):
            result = self.run_helper('st-copyout', data)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.base / 'out').read_text(), 'previous clipboard')

    def test_literal_prompt_and_menu_text(self):
        command = '[u@host]$ echo "$(touch ' + str(self.base / 'executed') + ')" \\x [.*]'
        data = command + '\ncontains [u@host]$ in output\n[u@host]$suffix is output\n\tspaces  \n[u@host]$ \n'
        result = self.run_helper('st-copyout', data, CHOICE='[1] ' + command)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(),
                         'contains [u@host]$ in output\n[u@host]$suffix is output\n\tspaces  \n')
        self.assertFalse((self.base / 'executed').exists())

    def test_prompt_boundaries(self):
        data = 'ignored prefix\n$ first\none\n$ \n$ second\ntwo\n$ pending'
        result = self.run_helper('st-copyout', data, CHOICE='[2] $ second')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), 'two\n')
        self.assertEqual((self.base / 'menu-input').read_text(), '[2] $ second\n[1] $ first')

    def test_unicode_and_nul(self):
        result = self.run_helper('st-copyout', 'λ echo 界\n界\0 café\nλ ')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), '界 café\n')

    def test_optional_font_wrapper(self):
        self.mock('dmenu-font', 'echo wrapper > "$OUT.wrapper"; exec dmenu "$@"')
        for helper, data in [('st-copyout', '$ hello\noutput\n$ \n'),
                             ('st-urlhandler', 'https://example.org\n')]:
            result = self.run_helper(helper, data)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.base / 'out.wrapper').read_text(), 'wrapper\n')
            (self.base / 'out.wrapper').unlink()

    def test_paths_and_recency(self):
        data = 'old ~/.bashrc /etc/var/log/foo\nmydir/myfile ./local ../parent ~/.bashrc\n'
        result = self.run_helper('st-pathcopy', data, CHOICE='~/.bashrc')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), '~/.bashrc')
        self.assertEqual((self.base / 'menu-input').read_text(),
                         '~/.bashrc\n../parent\n./local\nmydir/myfile\n/etc/var/log/foo')
        self.assertEqual((self.base / 'out.args').read_text().strip(), '-selection clipboard')

    def test_ls_classified_directories(self):
        data = ('total 8\n'
                'drwxr-xr-x 2 user user 4096 Oct 18 12:00 src/\n'
                '-rw-r--r-- 1 user user   42 Oct 18 12:00 README\n'
                '-rwxr-xr-x 1 user user   42 Oct 18 12:00 script*\n'
                'drwxr-xr-x 2 user user 4096 Oct 18 12:00 "my docs/"\n')
        for choice in ('src/', 'my docs/'):
            result = self.run_helper('st-pathcopy', data, CHOICE=choice)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.base / 'out').read_text(), choice)
            self.assertEqual((self.base / 'menu-input').read_text(), 'my docs/\nsrc/')

    def test_quoted_paths(self):
        data = '\"/tmp/my file\" \'my dir/界.txt\' \"/tmp/a[1](x).txt\"\n'
        for choice in ('/tmp/my file', 'my dir/界.txt', '/tmp/a[1](x).txt'):
            result = self.run_helper('st-pathcopy', data, CHOICE=choice)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.base / 'out').read_text(), choice)

    def test_path_punctuation_and_urls(self):
        data = ('path=/etc/config; (/tmp/foo), [../bar]. <~/.bashrc>\n'
                'https://example.org/?p=/wrong&x=~/bad www.example.org/file '
                'url=file:///tmp/wrong 2026/10/22 1/2 barefile\n')
        result = self.run_helper('st-pathcopy', data, CHOICE='/etc/config')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'menu-input').read_text(),
                         '~/.bashrc\n../bar\n/tmp/foo\n/etc/config')

    def test_path_copy_never_executes(self):
        path = '/tmp/$(touch ' + str(self.base / 'executed') + ')'
        result = self.run_helper('st-pathcopy', "'" + path + "'", CHOICE=path)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out').read_text(), path)
        self.assertFalse((self.base / 'executed').exists())

    def test_path_cancellation_and_invalid_input(self):
        (self.base / 'out').write_text('unchanged')
        for data, env in (
                ('/does/not/exist', {'CANCEL': '1'}),
                ('/does/not/exist', {'CHOICE': ''}),
                ('/does/not/exist', {'CHOICE': '/not/a/detected/path'}),
                ('no paths here 1/2 2026/10/22 https://example.org/a', {}),
                ('"/unfinished quoted path', {})):
            result = self.run_helper('st-pathcopy', data, **env)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.base / 'out').read_text(), 'unchanged')

    def test_path_font_wrapper_and_xsel(self):
        self.mock('dmenu-font', 'echo wrapper > "$OUT.wrapper"; exec dmenu "$@"')
        (self.base / 'xclip').rename(self.base / 'xsel')
        result = self.run_helper('st-pathcopy', '/does/not/exist')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out.wrapper').read_text(), 'wrapper\n')
        self.assertEqual((self.base / 'out.args').read_text().strip(), '-ib')
        self.assertEqual((self.base / 'out').read_text(), '/does/not/exist')

    def test_path_missing_clipboard(self):
        (self.base / 'xclip').unlink()
        result = self.run_helper('st-pathcopy', '~/.bashrc')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('install xclip or xsel', result.stderr)

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
        result = self.run_helper('st-copyout', '$ hello\noutput\n$ \n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'out.args').read_text().strip(), '-ib')
        self.assertEqual((self.base / 'out').read_text(), 'output\n')

    def test_missing_clipboard(self):
        (self.base / 'xclip').unlink()
        result = self.run_helper('st-copyout', '$ hello\noutput\n$ \n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('install xclip or xsel', result.stderr)

    def test_no_selection(self):
        for helper, data in [('st-copyout', self.HISTORY), ('st-urlhandler', 'no URLs'),
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
