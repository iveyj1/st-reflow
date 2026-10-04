# st-reflow: changes and cloc since the fork

Committed history through `80004e5de14b07f239047b2a56a76df6899c800a`; cloc **2.10**.

## Provenance and method

- Upstream: https://git.suckless.org/st.
- Original fork baseline: `d63b9eb90245926b531bd54b1d591adb96613e70`.
- Upstream HEAD verified during this review: `04ce0d643ed17793803e8516f4c9a5b13b93c400`.
- The original fork point is st 0.9.2 (d63b9eb), not the current merge-base 04ce0d6. Upstream fixes were subsequently merged at 405bd3e and fa6e1cb; using the current merge-base would hide the initial scrollback/reflow changes.
- Totals since the original fork include both local changes and explicitly identified upstream merges. Side-commit tables enumerate the imported upstream changes.
- Generated buildinfo.h is excluded even during the short period it was tracked. boxdraw_data.h is maintained source and is included.
- Commit b8e8aa0 is titled merge but has only one parent; it adds build metadata/splash changes, not a Git merge.
- Main tables follow first-parent history, oldest first; dates are author dates. Merge rows include their complete first-parent delta, including conflict resolutions. Side commits are detailed separately, not added again to totals.
- C = runtime .c/.h files; one effective configuration header (tracked config.h/blocks.h, otherwise its .def.h template). Support C = test/diagnostic C sources. Runtime scripts are counted separately from build/test/update scripts.
- cloc code lines exclude blanks/comments. Deltas are signed net changes vs the preceding row, not diff insertion/deletion counts. Zero does not imply no behavior change. Embedded shell strings in C count as C; shell help heredocs follow cloc classification.
- Build = Makefile/config.mk. Docs, terminfo, binaries, images, generated buildinfo.h, unused configuration templates, and reporting files under scripts/ are excluded. --skip-uniqueness prevents duplicate-file suppression.
- Historical counts use Git blobs. WORKTREE, when present, includes staged/unstaged changes and nonignored untracked regular files; delta is vs HEAD, never folded into committed history. Report-only edits do not create a WORKTREE row.
- Requires Python 3.9+, Git and cloc. Regenerate: `python3 scripts/update_cloc_by_commit.py`. Validate freshness: `python3 scripts/update_cloc_by_commit.py --check`.
- Regeneration is offline: upstream provenance records the review-time verification, not a fresh network check.
- Add reviewed descriptions to scripts/cloc-history.json when a commit subject is unclear. Otherwise new commits automatically use their subjects. Keep baseline fixed; do not move it to a later upstream merge-base.

## Runtime changes

| Commit | Date | C code | Δ C | Script code | Δ scripts | Change |
|---|---|---:|---:|---:|---:|---|
| `d63b9eb` | 2024-04-05 | 4429 | — | 0 | — | Upstream st 0.9.2; original parent of the first local scrollback patch. |
| `e0f95a1` | 2026-08-05 | 4492 | +63 | 0 | 0 | Apply scrollback patch for st 0.9.2 |
| `342798b` | 2026-08-05 | 4788 | +296 | 0 | 0 | Apply scrollback reflow patch for st 0.9.2 |
| `405bd3e` | 2026-08-05 | 4815 | +27 | 0 | 0 | Merge upstream st 0.9.3-era fixes through 688f70a into the scrollback/reflow build. |
| `fa6e1cb` | 2026-08-05 | 4819 | +4 | 0 | 0 | Merge upstream async-signal-safe SIGCHLD error handling (04ce0d6). |
| `1748af1` | 2026-08-05 | 4817 | -2 | 0 | 0 | Apply bold-is-not-bright patch |
| `d01c992` | 2026-08-05 | 4827 | +10 | 0 | 0 | Configure font/colors/geometry and keyboard/mouse scroll bindings. |
| `f04f23e` | 2026-08-05 | 4829 | +2 | 0 | 0 | Ignore synchronized-output mode sequences (not synchronized rendering support). |
| `78bcc4b` | 2026-08-05 | 4832 | +3 | 0 | 0 | Apply anysize patch |
| `af02b0a` | 2026-08-05 | 4855 | +23 | 0 | 0 | Port alpha patch to the reflow build |
| `eb709eb` | 2026-08-05 | 4870 | +15 | 0 | 0 | Adjust background opacity on focus changes |
| `26ae28f` | 2026-08-05 | 4959 | +89 | 0 | 0 | Load configuration from X resources at startup |
| `755913c` | 2026-08-05 | 5281 | +322 | 0 | 0 | Port boxdraw patch to the reflow build |
| `e1ab2ba` | 2026-08-05 | 5281 | 0 | 0 | 0 | Document patch provenance and ignore build artifacts. |
| `cbae263` | 2026-08-05 | 5288 | +7 | 0 | 0 | Implement CSI 3 J erase-saved-lines |
| `eaa6296` | 2026-08-05 | 5308 | +20 | 0 | 0 | Detach application redraws from reflowed line prefixes |
| `519a104` | 2026-08-05 | 5316 | +8 | 0 | 0 | Handle modern terminal capability probes quietly (DECRQM, kitty flags, modifyOtherKeys). |
| `a4bd5de` | 2026-08-05 | 5316 | 0 | 0 | 0 | Document reflow redraw and terminal probe fixes |
| `b946e5f` | 2026-08-05 | 5296 | -20 | 0 | 0 | Revert "Detach application redraws from reflowed line prefixes" |
| `22c344f` | 2026-08-05 | 5296 | 0 | 0 | 0 | Remove documentation for reverted redraw heuristic |
| `b35a7d1` | 2026-08-06 | 5312 | +16 | 0 | 0 | Let line editors repaint the active cursor line after reflow; retain output/history reflow. |
| `6216a45` | 2026-08-06 | 5312 | 0 | 0 | 0 | Document the reflow rebuild and publication workflow |
| `f0b2bff` | 2026-08-06 | 5312 | 0 | 0 | 0 | Preserve the original reflow rebuild proposal |
| `11a5d3e` | 2026-08-06 | 5360 | +48 | 0 | 0 | Add an inconspicuous startup version overlay |
| `bbfb77f` | 2026-08-06 | 5482 | +122 | 0 | 0 | Add an internal Alt-F4 close warning |
| `26d9fa8` | 2026-08-09 | 5543 | +61 | 0 | 0 | Add reflow-aware external pipe helpers |
| `2975ff7` | 2026-08-09 | 5749 | +206 | 0 | 0 | Add Vim-style keyboard copy mode |
| `52156bd` | 2026-08-10 | 5772 | +23 | 0 | 0 | Draw a dedicated cursor in keyboard copy mode |
| `2628ce5` | 2026-08-10 | 5774 | +2 | 0 | 0 | Recognize OSC 8 sequences quietly; no clickable-link implementation. |
| `62578fa` | 2026-08-10 | 5774 | 0 | 0 | 0 | Advertise scrollback clearing in terminfo (excluded from code counts). |
| `3b1d16e` | 2026-08-10 | 5774 | 0 | 0 | 0 | Fix multi-page keyboard copy selections |
| `1939b73` | 2026-08-10 | 5774 | 0 | 0 | 0 | Enable the previously ported geometric box drawing. |
| `3d2d0f2` | 2026-08-10 | 5774 | 0 | 0 | 0 | Add manual table-rendering test document (excluded from code counts). |
| `55cff8f` | 2026-08-10 | 5774 | 0 | 0 | 0 | Set opacity to 1.0; add b build/optional-install script. |
| `73d48ab` | 2026-08-10 | 5774 | 0 | 0 | 0 | Enable allowwindowops for OSC 52 clipboard copying. |
| `708c6d2` | 2026-08-12 | 5774 | 0 | 0 | 0 | Switch configured font to JetBrainsMono. |
| `c8cf998` | 2026-08-12 | 5774 | 0 | 0 | 0 | Reduce configured font size. |
| `a307bdf` | 2026-08-24 | 5776 | +2 | 0 | 0 | Add Super+Shift font zoom shortcuts. |
| `b21f09f` | 2026-08-24 | 5776 | 0 | 0 | 0 | Track config.h instead of ignoring it; count the effective header only, not both copies. |
| `abb7512` | 2026-08-31 | 5776 | 0 | 0 | 0 | Consolidate docs/patch history, add maintenance notes, revise distribution file list. |
| `b8e8aa0` | 2026-09-07 | 5777 | +1 | 0 | 0 | Generate Git build metadata; include it in splash text, increase timeout, revise packaging. |
| `40552e7` | 2026-09-07 | 5777 | 0 | 0 | 0 | Temporarily track generated buildinfo.h (excluded from counts). |
| `05d9caf` | 2026-09-07 | 5777 | 0 | 0 | 0 | Stop tracking generated buildinfo.h; update ignores. |
| `59a0469` | 2026-09-07 | 5778 | +1 | 0 | 0 | Synchronize effective config.h with updated splash configuration. |
| `50e5603` | 2026-09-09 | 5836 | +58 | 0 | 0 | Normalize paste line endings; add clean-copy/yank actions and shortcut documentation. |
| `72fd354` | 2026-09-09 | 5847 | +11 | 0 | 0 | Add rectangular keyboard selection with Ctrl+v. |
| `79f4ce6` | 2026-09-10 | 6033 | +186 | 0 | 0 | Add word/WORD motions, first/last printable positions, counts and history-row jumps. |
| `db9f6b5` | 2026-09-11 | 6033 | 0 | 41 | +41 | Ship st-copyout/st-urlhandler, clipboard/browser fallbacks and tests; repair install/dist packaging. |
| `9a09e8c` | 2026-09-13 | 6033 | 0 | 41 | 0 | Document Mint build dependencies. |
| `b809f33` | 2026-09-13 | 6033 | 0 | 45 | +4 | Align font defaults and make helper menus use dmenu-font when available; test wrapper integration. |
| `86bb421` | 2026-09-13 | 6033 | 0 | 45 | 0 | Add suck multi-project rebuild/update script (support tooling). |
| `80004e5` | 2026-09-30 | 6039 | +6 | 45 | 0 | Exit copy mode/clear highlights after successful explicit copy while retaining clipboard and scroll position; add regression tests. |
| `WORKTREE` | uncommitted | 6039 | 0 | 45 | 0 | Pending changes vs HEAD: AGENTS.md, README.md, scripts/cloc-history.json, scripts/test_cloc_history.py, scripts/update_cloc_by_commit.py |

## Supporting code and build files

| Commit | Support C | Δ C | Support scripts | Δ scripts | Build | Δ build | Combined code | Δ combined |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `d63b9eb` | 0 | — | 0 | — | 51 | — | 4480 | — |
| `e0f95a1` | 0 | 0 | 0 | 0 | 51 | 0 | 4543 | +63 |
| `342798b` | 0 | 0 | 0 | 0 | 51 | 0 | 4839 | +296 |
| `405bd3e` | 0 | 0 | 0 | 0 | 51 | 0 | 4866 | +27 |
| `fa6e1cb` | 0 | 0 | 0 | 0 | 51 | 0 | 4870 | +4 |
| `1748af1` | 0 | 0 | 0 | 0 | 51 | 0 | 4868 | -2 |
| `d01c992` | 0 | 0 | 0 | 0 | 51 | 0 | 4878 | +10 |
| `f04f23e` | 0 | 0 | 0 | 0 | 51 | 0 | 4880 | +2 |
| `78bcc4b` | 0 | 0 | 0 | 0 | 51 | 0 | 4883 | +3 |
| `af02b0a` | 0 | 0 | 0 | 0 | 51 | 0 | 4906 | +23 |
| `eb709eb` | 0 | 0 | 0 | 0 | 51 | 0 | 4921 | +15 |
| `26ae28f` | 0 | 0 | 0 | 0 | 51 | 0 | 5010 | +89 |
| `755913c` | 0 | 0 | 0 | 0 | 52 | +1 | 5333 | +323 |
| `e1ab2ba` | 0 | 0 | 0 | 0 | 52 | 0 | 5333 | 0 |
| `cbae263` | 0 | 0 | 0 | 0 | 52 | 0 | 5340 | +7 |
| `eaa6296` | 0 | 0 | 0 | 0 | 52 | 0 | 5360 | +20 |
| `519a104` | 0 | 0 | 0 | 0 | 52 | 0 | 5368 | +8 |
| `a4bd5de` | 0 | 0 | 0 | 0 | 52 | 0 | 5368 | 0 |
| `b946e5f` | 0 | 0 | 0 | 0 | 52 | 0 | 5348 | -20 |
| `22c344f` | 0 | 0 | 0 | 0 | 52 | 0 | 5348 | 0 |
| `b35a7d1` | 0 | 0 | 0 | 0 | 52 | 0 | 5364 | +16 |
| `6216a45` | 0 | 0 | 0 | 0 | 52 | 0 | 5364 | 0 |
| `f0b2bff` | 0 | 0 | 0 | 0 | 52 | 0 | 5364 | 0 |
| `11a5d3e` | 0 | 0 | 0 | 0 | 52 | 0 | 5412 | +48 |
| `bbfb77f` | 0 | 0 | 0 | 0 | 52 | 0 | 5534 | +122 |
| `26d9fa8` | 0 | 0 | 0 | 0 | 52 | 0 | 5595 | +61 |
| `2975ff7` | 0 | 0 | 0 | 0 | 52 | 0 | 5801 | +206 |
| `52156bd` | 0 | 0 | 0 | 0 | 52 | 0 | 5824 | +23 |
| `2628ce5` | 0 | 0 | 0 | 0 | 52 | 0 | 5826 | +2 |
| `62578fa` | 0 | 0 | 0 | 0 | 52 | 0 | 5826 | 0 |
| `3b1d16e` | 0 | 0 | 0 | 0 | 52 | 0 | 5826 | 0 |
| `1939b73` | 0 | 0 | 0 | 0 | 52 | 0 | 5826 | 0 |
| `3d2d0f2` | 0 | 0 | 0 | 0 | 52 | 0 | 5826 | 0 |
| `55cff8f` | 0 | 0 | 7 | +7 | 52 | 0 | 5833 | +7 |
| `73d48ab` | 0 | 0 | 7 | 0 | 52 | 0 | 5833 | 0 |
| `708c6d2` | 0 | 0 | 7 | 0 | 52 | 0 | 5833 | 0 |
| `c8cf998` | 0 | 0 | 7 | 0 | 52 | 0 | 5833 | 0 |
| `a307bdf` | 0 | 0 | 7 | 0 | 52 | 0 | 5835 | +2 |
| `b21f09f` | 0 | 0 | 7 | 0 | 52 | 0 | 5835 | 0 |
| `abb7512` | 0 | 0 | 7 | 0 | 52 | 0 | 5835 | 0 |
| `b8e8aa0` | 0 | 0 | 7 | 0 | 67 | +15 | 5851 | +16 |
| `40552e7` | 0 | 0 | 7 | 0 | 67 | 0 | 5851 | 0 |
| `05d9caf` | 0 | 0 | 7 | 0 | 67 | 0 | 5851 | 0 |
| `59a0469` | 0 | 0 | 7 | 0 | 67 | 0 | 5852 | +1 |
| `50e5603` | 0 | 0 | 7 | 0 | 67 | 0 | 5910 | +58 |
| `72fd354` | 0 | 0 | 7 | 0 | 67 | 0 | 5921 | +11 |
| `79f4ce6` | 0 | 0 | 7 | 0 | 67 | 0 | 6107 | +186 |
| `db9f6b5` | 0 | 0 | 82 | +75 | 73 | +6 | 6229 | +122 |
| `9a09e8c` | 0 | 0 | 82 | 0 | 73 | 0 | 6229 | 0 |
| `b809f33` | 0 | 0 | 90 | +8 | 73 | 0 | 6241 | +12 |
| `86bb421` | 0 | 0 | 125 | +35 | 73 | 0 | 6276 | +35 |
| `80004e5` | 0 | 0 | 157 | +32 | 74 | +1 | 6315 | +39 |
| `WORKTREE` | 0 | 0 | 157 | 0 | 74 | 0 | 6315 | 0 |

Combined includes all five counted categories; excluded files remain excluded.

## Baseline → committed HEAD totals

| Category | Code baseline → HEAD (net) | Blank baseline → HEAD | Comment baseline → HEAD |
|---|---:|---:|---:|
| C | 4429 → 6039 (+1610) | 606 → 795 | 436 → 583 |
| Runtime scripts | 0 → 45 (+45) | 0 → 0 | 0 → 3 |
| Support C | 0 → 0 (0) | 0 → 0 | 0 → 0 |
| Support scripts | 0 → 157 (+157) | 0 → 31 | 0 → 65 |
| Build | 51 → 74 (+23) | 21 → 24 | 15 → 17 |

## Commits integrated by merges

Counts below are each side commit’s snapshot and delta vs its own first parent. These are **not additive** with the main tables.

### Merge `405bd3e`

| Side commit | C (Δ) | Runtime scripts (Δ) | Support C (Δ) | Support scripts (Δ) | Build (Δ) | Subject |
|---|---:|---:|---:|---:|---:|---|
| `5dbcca4` | 4432 (+3) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | support colons in SGR character attributes |
| `a0274bc` | 4435 (+3) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | fix BadMatch error when embedding on some windows |
| `6009e6e` | 4435 (0) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | Clear screen: Fix edge case |
| `98610fc` | 4439 (+4) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | Do not interpret CSI ? u as DECRC |
| `f114bce` | 4442 (+3) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | Eat up "CSI 58" sequences |
| `d6c4318` | 4455 (+13) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | Support OSC 110, 111, and 112 for resetting colors |
| `5a4666c` | 4455 (0) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | add a few comments |
| `6e97047` | 4455 (0) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | bump version to 0.9.3 |
| `0723b7e` | 4456 (+1) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | Disable bracked paste in reset |
| `688f70a` | 4458 (+2) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | st: guard tsetdirt() against zero-sized terminal |

### Merge `fa6e1cb`

| Side commit | C (Δ) | Runtime scripts (Δ) | Support C (Δ) | Support scripts (Δ) | Build (Δ) | Subject |
|---|---:|---:|---:|---:|---:|---|
| `04ce0d6` | 4462 (+4) | 0 (0) | 0 (0) | 0 (0) | 51 (0) | fix async-unsafe error paths in sigchld() |

## Files counted at committed HEAD

- **C:** `arg.h`, `boxdraw.c`, `boxdraw_data.h`, `config.h`, `st.c`, `st.h`, `win.h`, `x.c`.
- **Runtime scripts:** `st-copyout`, `st-urlhandler`.
- **Support C:** (none).
- **Support scripts:** `b`, `suck`, `tests/copy_exit.py`, `tests/helpers.py`.
- **Build:** `Makefile`, `config.mk`.

## Latest change

WORKTREE — Pending changes vs HEAD: AGENTS.md, README.md, scripts/cloc-history.json, scripts/test_cloc_history.py, scripts/update_cloc_by_commit.py
C: 6039 (0); Runtime scripts: 45 (0); Support C: 0 (0); Support scripts: 157 (0); Build: 74 (0)

Maintenance: regenerate after each modification and again after committing (or switching branches); include the latest code/script totals and net deltas in the change summary. Reporting-only changes legitimately have zero measured delta.
