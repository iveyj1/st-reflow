#!/usr/bin/env python3
"""Generate reproducible Git + working-tree cloc history. Python 3.9+, git, cloc."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

GROUPS = ('C', 'Runtime scripts', 'Support C', 'Support scripts', 'Build')
REPORT = 'scripts/cloc_by_commit.md'
METRICS = ('code', 'blank', 'comment')


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def text(root, *args):
    return git(root, *args).decode().strip()


def snapshot(root, revision):
    if revision is not None:
        entries = git(root, 'ls-tree', '-rz', revision).split(b'\0')
        files = {}
        for entry in filter(None, entries):
            metadata, name = entry.split(b'\t', 1)
            mode, kind, oid = metadata.split()
            if kind == b'blob' and mode in (b'100644', b'100755'):
                files[name.decode()] = git(root, 'cat-file', 'blob', oid.decode())
        return files
    names = git(root, 'ls-files', '-z', '--cached', '--others', '--exclude-standard')
    return {name: (root / name).read_bytes()
            for name in set(names.decode().split('\0')) if name
            and (root / name).is_file() and not (root / name).is_symlink()}


def classify(files, config):
    groups = {name: {} for name in GROUPS}
    unused = set(config.get('exclude', []))
    for active, template in config.get('config_pairs', []):
        unused.add(template if active in files else active)
    for name, content in files.items():
        # Reporting machinery never contributes to the numbers it reports.
        if name.startswith('scripts/') or name in unused:
            continue
        support = name.startswith('tests/') or name in config.get('support', [])
        suffix = Path(name).suffix
        if suffix in ('.c', '.h'):
            group = 'Support C' if support else 'C'
        elif suffix in ('.py', '.sh', '.bash', '.pl') or content.startswith(b'#!'):
            group = 'Support scripts' if support else 'Runtime scripts'
        elif name in ('Makefile', 'config.mk'):
            group = 'Build'
        else:
            continue
        groups[group][name] = content
    return groups


class Counter:
    def __init__(self):
        self.cache = {}

    def count(self, files):
        if not files:
            return dict.fromkeys(METRICS, 0)
        digest = hashlib.sha256()
        for name, data in sorted(files.items()):
            digest.update(name.encode() + b'\0' + str(len(data)).encode() + b'\0' + data)
        key = digest.digest()
        if key not in self.cache:
            with tempfile.TemporaryDirectory(prefix='fork-cloc-') as directory:
                for name, data in files.items():
                    path = Path(directory) / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
                result = json.loads(subprocess.check_output(
                    ['cloc', '--quiet', '--json', '--skip-uniqueness', directory], text=True))
                if result.get('SUM', {}).get('nFiles') != len(files):
                    raise RuntimeError('cloc did not recognize every selected file: ' + ', '.join(files))
                self.cache[key] = {m: result['SUM'][m] for m in METRICS}
        return self.cache[key]

    def stats(self, files, config):
        return {group: self.count(selected) for group, selected in classify(files, config).items()}


def signed(value):
    return f'{value:+d}' if value else '0'


def escape(value):
    return value.replace('|', r'\|').replace('\n', ' ')


def change_summary(new, old):
    return '; '.join(f"{g}: {new[g]['code']} ({signed(new[g]['code'] - old[g]['code'])})"
                     for g in GROUPS)


def render(root, config):
    base = text(root, 'rev-parse', config['baseline'])
    head = text(root, 'rev-parse', 'HEAD')
    chain = text(root, 'rev-list', '--first-parent', head).splitlines()
    if base not in chain:
        raise ValueError('Baseline must be on the first-parent history of HEAD')
    revisions = list(reversed(chain[:chain.index(base) + 1]))
    counter = Counter()
    rows, snapshots = [], {}
    for rev in revisions:
        files = snapshot(root, rev)
        stats = counter.stats(files, config)
        snapshots[rev] = stats
        subject = config.get('changes', {}).get(rev[:7], text(root, 'show', '-s', '--format=%s', rev))
        if rev == base:
            subject = config['baseline_description']
        rows.append((rev[:7], text(root, 'show', '-s', '--format=%ad', '--date=short', rev), stats, subject))
    committed = snapshot(root, head)
    working = snapshot(root, None)
    # Excluding the report itself makes repeated generation stable, even before commit.
    pending = sorted(name for name in committed.keys() | working.keys()
                     if name != REPORT and committed.get(name) != working.get(name))
    if pending:
        rows.append(('WORKTREE', 'uncommitted', counter.stats(working, config),
                     'Pending changes vs HEAD: ' + ', '.join(pending)))
    version = subprocess.check_output(['cloc', '--version'], text=True).strip()
    lines = [f"# {config['project']}: changes and cloc since the fork", '',
             f'Committed history through `{head}`; cloc **{version}**.', '',
             '## Provenance and method', '',
             f"- Upstream: {config['upstream_url']}.",
             f'- Original fork baseline: `{base}`.',
             f"- Upstream HEAD verified during this review: `{config['upstream_head']}`."]
    lines += ['- ' + note for note in config.get('notes', [])]
    lines += [
        '- Main tables follow first-parent history, oldest first; dates are author dates. Merge rows include their complete first-parent delta, including conflict resolutions. Side commits are detailed separately, not added again to totals.',
        '- C = runtime .c/.h files; one effective configuration header (tracked config.h/blocks.h, otherwise its .def.h template). Support C = test/diagnostic C sources. Runtime scripts are counted separately from build/test/update scripts.',
        '- cloc code lines exclude blanks/comments. Deltas are signed net changes vs the preceding row, not diff insertion/deletion counts. Zero does not imply no behavior change. Embedded shell strings in C count as C; shell help heredocs follow cloc classification.',
        '- Build = Makefile/config.mk. Docs, terminfo, binaries, images, generated buildinfo.h, unused configuration templates, and reporting files under scripts/ are excluded. --skip-uniqueness prevents duplicate-file suppression.',
        '- Historical counts use Git blobs. WORKTREE, when present, includes staged/unstaged changes and nonignored untracked regular files; delta is vs HEAD, never folded into committed history. Report-only edits do not create a WORKTREE row.',
        '- Requires Python 3.9+, Git and cloc. Regenerate: `python3 scripts/update_cloc_by_commit.py`. Validate freshness: `python3 scripts/update_cloc_by_commit.py --check`.',
        '- Regeneration is offline: upstream provenance records the review-time verification, not a fresh network check.',
        '- Add reviewed descriptions to scripts/cloc-history.json when a commit subject is unclear. Otherwise new commits automatically use their subjects. Keep baseline fixed; do not move it to a later upstream merge-base.', '',
        '## Runtime changes', '',
        '| Commit | Date | C code | Δ C | Script code | Δ scripts | Change |',
        '|---|---|---:|---:|---:|---:|---|']
    previous = None
    for rev, date, stats, subject in rows:
        delta = lambda group: '—' if previous is None else signed(stats[group]['code'] - previous[group]['code'])
        lines.append(f"| `{rev}` | {date} | {stats['C']['code']} | {delta('C')} | "
                     f"{stats['Runtime scripts']['code']} | {delta('Runtime scripts')} | {escape(subject)} |")
        previous = stats
    lines += ['', '## Supporting code and build files', '',
              '| Commit | Support C | Δ C | Support scripts | Δ scripts | Build | Δ build | Combined code | Δ combined |',
              '|---|---:|---:|---:|---:|---:|---:|---:|---:|']
    previous = None
    for rev, _, stats, _ in rows:
        cells = [f'`{rev}`']
        for group in GROUPS[2:]:
            cells += [str(stats[group]['code']), '—' if previous is None else signed(stats[group]['code'] - previous[group]['code'])]
        total = sum(s['code'] for s in stats.values())
        cells += [str(total), '—' if previous is None else signed(total - sum(s['code'] for s in previous.values()))]
        lines.append('| ' + ' | '.join(cells) + ' |')
        previous = stats
    lines += ['', 'Combined includes all five counted categories; excluded files remain excluded.', '',
              '## Baseline → committed HEAD totals', '',
              '| Category | Code baseline → HEAD (net) | Blank baseline → HEAD | Comment baseline → HEAD |',
              '|---|---:|---:|---:|']
    for group in GROUPS:
        old, new = snapshots[base][group], snapshots[head][group]
        lines.append(f"| {group} | {old['code']} → {new['code']} ({signed(new['code'] - old['code'])}) | "
                     f"{old['blank']} → {new['blank']} | {old['comment']} → {new['comment']} |")
    lines += ['', '## Commits integrated by merges', '',
              'Counts below are each side commit’s snapshot and delta vs its own first parent. These are **not additive** with the main tables.']
    for rev in revisions[1:]:
        parents = text(root, 'show', '-s', '--format=%P', rev).split()
        if len(parents) < 2:
            continue
        side = text(root, 'rev-list', '--reverse', '--topo-order', rev, '^' + parents[0]).splitlines()
        side = [s for s in side if s != rev]
        lines += ['', f'### Merge `{rev[:7]}`', '',
                  '| Side commit | C (Δ) | Runtime scripts (Δ) | Support C (Δ) | Support scripts (Δ) | Build (Δ) | Subject |',
                  '|---|---:|---:|---:|---:|---:|---|']
        for commit in side:
            current = counter.stats(snapshot(root, commit), config)
            parent = counter.stats(snapshot(root, commit + '^'), config)
            cells = [f'`{commit[:7]}`'] + [f"{current[g]['code']} ({signed(current[g]['code'] - parent[g]['code'])})" for g in GROUPS]
            cells += [escape(text(root, 'show', '-s', '--format=%s', commit))]
            lines.append('| ' + ' | '.join(cells) + ' |')
    if not any(len(text(root, 'show', '-s', '--format=%P', rev).split()) > 1 for rev in revisions[1:]):
        lines += ['', 'No post-fork merges.']
    lines += ['', '## Files counted at committed HEAD', '']
    for group, files in classify(committed, config).items():
        lines.append(f'- **{group}:** ' + (', '.join(f'`{f}`' for f in sorted(files)) or '(none)') + '.')
    latest = rows[-1]
    old = rows[-2][2] if len(rows) > 1 else latest[2]
    summary = f'{latest[0]} — {latest[3]}\n' + change_summary(latest[2], old)
    lines += ['', '## Latest change', '', summary, '',
              'Maintenance: regenerate after each modification and again after committing (or switching branches); include the latest code/script totals and net deltas in the change summary. Reporting-only changes legitimately have zero measured delta.', '']
    return '\n'.join(lines), summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='exit nonzero if the saved report is stale')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    config = json.loads((root / 'scripts/cloc-history.json').read_text())
    report, summary = render(root, config)
    output = root / REPORT
    if args.check:
        if not output.exists() or output.read_text() != report:
            parser.exit(1, 'Report is stale; run python3 scripts/update_cloc_by_commit.py\n')
        print('Report is current.')
    else:
        output.write_text(report)
        print(f'Wrote {output}')
    print(summary)


if __name__ == '__main__':
    main()
