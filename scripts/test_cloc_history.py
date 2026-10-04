#!/usr/bin/env python3
"""Isolated Git fixtures; no modifications to the real project history."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
SCRIPT = Path(__file__).with_name('update_cloc_by_commit.py')
spec = importlib.util.spec_from_file_location('history', SCRIPT)
history = importlib.util.module_from_spec(spec)
spec.loader.exec_module(history)


class History(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.git('init', '-q', '-b', 'main')
        self.git('config', 'user.name', 'History test')
        self.git('config', 'user.email', 'test@example.invalid')
        self.git('config', 'commit.gpgsign', 'false')
        self.put('.gitignore', 'ignored.sh\n')
        self.put('main.c', 'int main(void) { return 0; }\n')
        self.put('config.def.h', '#define VALUE 1\n')
        self.put('run', '#!/bin/sh\necho hello\n')
        self.put('Makefile', 'all:\n\ttrue\n')
        self.commit('baseline')
        self.base = self.git('rev-parse', 'HEAD')
        self.config = dict(project='fixture', baseline=self.base,
                           baseline_description='Fixture baseline',
                           upstream_url='fixture.invalid', upstream_head=self.base,
                           config_pairs=[['config.h', 'config.def.h']],
                           support=['b', 'diagnostic.c'], exclude=['buildinfo.h'])

    def git(self, *args):
        return history.text(self.root, *args)

    def put(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)

    def commit(self, subject):
        self.git('add', '.')
        self.git('commit', '-qm', subject)

    def test_categories_and_configuration_fallback(self):
        files = history.snapshot(self.root, self.base)
        files.update({'config.h': b'#define VALUE 2\n', 'buildinfo.h': b'int generated;\n',
                      'b': b'#!/bin/sh\ntrue\n', 'diagnostic.c': b'int test;\n',
                      'tests/test.py': b'print(1)\n', 'scripts/report.py': b'print(1)\n'})
        groups = history.classify(files, self.config)
        self.assertEqual(set(groups['C']), {'main.c', 'config.h'})
        self.assertEqual(set(groups['Runtime scripts']), {'run'})
        self.assertEqual(set(groups['Support scripts']), {'b', 'tests/test.py'})
        self.assertEqual(set(groups['Support C']), {'diagnostic.c'})
        self.assertEqual(set(groups['Build']), {'Makefile'})
        del files['config.h']
        self.assertIn('config.def.h', history.classify(files, self.config)['C'])

    def test_history_and_worktree(self):
        self.put('main.c', 'int extra;\nint main(void) { return 0; }\n')
        self.commit('add variable')
        clean, summary = history.render(self.root, self.config)
        self.assertNotIn('| `WORKTREE`', clean)
        self.assertIn('C: 3 (+1)', summary)
        self.put('main.c', 'int one;\nint two;\nint main(void) { return 0; }\n')
        self.git('add', 'main.c')  # staged and unstaged content are both covered
        self.put('run', '#!/bin/sh\necho hello\necho world\n')
        self.put('new.sh', '#!/bin/sh\ntrue\n')
        self.put('ignored.sh', '#!/bin/sh\nfalse\n')
        report, summary = history.render(self.root, self.config)
        self.assertIn('C: 4 (+1)', summary)
        self.assertIn('new.sh', summary)
        self.assertNotIn('ignored.sh', summary)
        self.assertIn('| `WORKTREE`', report)
        self.put(history.REPORT, report)
        self.assertEqual(history.render(self.root, self.config)[0], report)
        (self.root / 'run').unlink()
        deleted = history.classify(history.snapshot(self.root, None), self.config)
        self.assertNotIn('run', deleted['Runtime scripts'])
        # Git history remains unaffected by any of these local edits.
        self.assertEqual(history.snapshot(self.root, self.base)['main.c'].count(b'\n'), 1)

    def test_duplicate_files_and_net_deletion(self):
        counts = history.Counter().count({'a.c': b'int value;\n', 'b.c': b'int value;\n'})
        self.assertEqual(counts['code'], 2)
        (self.root / 'main.c').unlink()
        self.commit('remove source')
        _, summary = history.render(self.root, self.config)
        self.assertIn('C: 1 (-1)', summary)
        self.put('README', 'Documentation-only\n')
        _, summary = history.render(self.root, self.config)
        self.assertIn('C: 1 (0)', summary)

    def test_cli_check(self):
        self.put('scripts/cloc-history.json', json.dumps(self.config))
        shutil.copyfile(SCRIPT, self.root / 'scripts/update_cloc_by_commit.py')
        command = [sys.executable, str(self.root / 'scripts/update_cloc_by_commit.py')]
        subprocess.run(command, check=True, capture_output=True)
        subprocess.run(command + ['--check'], check=True, capture_output=True)
        self.put('README', 'Documentation-only change\n')
        self.assertEqual(subprocess.run(command + ['--check'], capture_output=True).returncode, 1)
        subprocess.run(command, check=True, capture_output=True)
        self.commit('save report and documentation')
        # The pending row must be replaced after committing, even for zero delta.
        self.assertEqual(subprocess.run(command + ['--check'], capture_output=True).returncode, 1)
        subprocess.run(command, check=True, capture_output=True)
        subprocess.run(command + ['--check'], check=True, capture_output=True)

    def test_merge_and_baseline_guard(self):
        self.git('checkout', '-qb', 'feature')
        self.put('side.c', 'int side;\n')
        self.commit('side feature')
        side = self.git('rev-parse', '--short', 'HEAD')
        self.git('checkout', '-q', 'main')
        self.put('README', 'Main branch\n')
        self.commit('docs')
        self.git('merge', '-q', '--no-ff', 'feature', '-m', 'merge feature')
        report, summary = history.render(self.root, self.config)
        self.assertIn('### Merge', report)
        self.assertIn(f'`{side}`', report)
        self.assertIn('C: 3 (+1)', summary)
        bad = dict(self.config, baseline=side)
        with self.assertRaises(ValueError):
            history.render(self.root, bad)


if __name__ == '__main__':
    unittest.main()
