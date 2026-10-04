# st-reflow agent notes

If you change keyboard/mouse shortcuts, update `../dwm/dwm-keymap` in the same change when the shortcut is user-facing. That keymap is opened with dwm `Super+Shift+K` and documents st shortcuts too.

Keep `config.def.h` and tracked `config.h` synchronized.

## Change-size history (required)

After each modification, run `python3 scripts/update_cloc_by_commit.py`.
`scripts/cloc_by_commit.md` is generated locally and gitignored; never stage it. Regenerate after committing or changing
branches to replace WORKTREE with the actual commit row. Do not create commits
without the user's request just to refresh the report.

Verify with `python3 scripts/update_cloc_by_commit.py --check`; test reporting
changes with `python3 scripts/test_cloc_history.py`. In the final response report
latest runtime C/script totals and net deltas, plus nonzero support C/scripts/build
deltas. Explicitly report zero for documentation/reporting-only changes.

Maintain the fixed baseline and scope in `scripts/cloc-history.json`, adding
reviewed descriptions when commit subjects are unclear. The baseline is the
original st 0.9.2 fork, NOT a later upstream merge-base. Preserve effective-header
and generated-buildinfo exclusions. WORKTREE includes staged/unstaged changes and
nonignored new files; never present those as part of an existing commit.
