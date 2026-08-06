# Proposal: Rebuild `st` Around Scrollback Reflow

## Summary

Rebuild this `st` configuration from a clean stock base rather than retrofit text reflow into the current Luke Smith-derived tree.

The proposed foundation is:

1. Stock `st` 0.9.2
2. The standard `scrollback` patch for 0.9.2
3. The corresponding `scrollback-reflow` patch
4. Stock fixes through `st` 0.9.3
5. A deliberately small set of additional patches, applied and tested one at a time

This makes reflow part of the terminal's fundamental history model. It avoids adapting reflow to the current `maxcol` resize behavior, local history traversal, vim-style browse mode, and other coupled modifications.

## Motivation

The current tree does not reflow text when its width changes. Its resize implementation retains the widest allocation seen so far through `Term.maxcol`:

```c
col = MAX(col, term.maxcol);
```

`term.col` records the visible width while rows remain allocated at `term.maxcol`. This preserves hidden columns but does not reconstruct logical lines. It also permits rows produced at different widths to coexist and makes a later reflow implementation more complicated.

The current tree additionally combines:

- Luke-style scrollback
- A circular history buffer
- `TLINE_HIST` traversal for external commands
- Local vim-style history browsing
- Selection modifications
- External pipe and URL helpers
- Alternate-screen behavior
- HarfBuzz shaping and box drawing

Retrofitting reflow would require reconciling all of these at once. Starting from the established scrollback/reflow patch set is lower risk and should leave a smaller, more maintainable result.

## Feasibility Check

The intended patch sequence was checked against this repository's stock history:

- Stock 0.9.2 commit: `d63b9eb`
- `st-scrollback-0.9.2.diff`
- `st-scrollback-reflow-0.9.2.diff`

The standard scrollback patch applies first, and the reflow patch then applies cleanly. The reflow patch is not standalone despite its filename; it expects the regular scrollback patch to have already been applied.

## Goals

- Reflow soft-wrapped primary-screen text when the terminal width changes.
- Preserve hard line breaks.
- Reflow scrollback and the visible primary screen as one body of content.
- Preserve cursor placement correctly across resize.
- Keep alternate-screen applications functional and allow them to repaint after `SIGWINCH`.
- Retain only custom features that provide clear value.
- Create a patch stack that can be understood, tested, and updated incrementally.
- Keep personal configuration separate from functional patches where practical.

## Non-goals

- Preserve every existing Luke modification.
- Preserve the local vim-style browse mode in the first version.
- Guarantee that an active selection survives a width change.
- Redesign history to use unlimited storage or count logical rather than physical lines.
- Reimplement reflow independently when an established patch is available.

## Proposed Patch Stack

### Stage 1: Core foundation

1. Check out stock `st` 0.9.2 at `d63b9eb`.
2. Apply `st-scrollback-0.9.2.diff`.
3. Apply `st-scrollback-reflow-0.9.2.diff`.
4. Build and test before adding any other feature.
5. Bring in stock changes through the current 0.9.3 base (`688f70a`), resolving overlaps carefully.

This stage establishes the terminal and history data model. No local history-dependent feature should be added until resize behavior is reliable.

### Stage 2: Low-risk behavior and configuration

Add these separately, with one commit and one test cycle per feature:

- Mouse and keyboard scrollback bindings
- Clipboard copy and paste bindings
- No-bright-on-bold behavior
- Preferred colors
- Preferred font and default geometry
- Font zoom shortcuts
- Synchronous-update handling, if not already present upstream

Configuration-only changes should remain configuration-only commits.

### Stage 3: Window-system patches

Consider adding:

- Anysize
- Alpha transparency
- Focus/unfocus alpha behavior
- Xresources

These primarily affect `x.c`, but alpha, border clearing, and anysize can interact. Add them in that order only after checking the current versions of their patches and their documented prerequisites.

The new build must not restore the existing `Term.maxcol` behavior. Anysize should handle unused window pixels in the X frontend rather than preserve hidden terminal columns.

### Stage 4: Rendering patches

Add and test individually:

1. Boxdraw
2. Explicit Font2 fallback, only if stock font fallback is inadequate
3. HarfBuzz/ligatures, only if still desired

Boxdraw and HarfBuzz both modify the drawing pipeline. They should not be introduced in the same commit.

Before restoring Font2, test whether stock fontconfig fallback handles Nerd Font symbols and emoji adequately. Omitting Font2 would reduce code and patch conflicts.

### Stage 5: History-dependent conveniences

Evaluate these only after reflow is stable:

- External pipe
- External-pipe-eternal behavior
- URL selection and opening helpers
- Command-output copy helper
- Clear-history behavior

These features must enumerate the reflow patch's history representation correctly. Existing `TLINE_HIST` code should not be copied without review.

### Stage 6: Optional local features

The following should be omitted initially:

- Vim-style normal browse mode
- Close-warning dialogs
- Miscellaneous Luke scripts and defaults that are not regularly used

Vim-style browse mode stores physical screen coordinates and is tightly coupled to scrollback and selection. Restoring it would require explicit reflow-aware cursor and selection mapping. It should be a separate follow-up project, not part of the foundational rebuild.

## Initial Recommended Feature Set

The first usable release should contain:

- Scrollback
- Scrollback reflow
- Mouse and keyboard scrolling
- Clipboard shortcuts
- Anysize
- Alpha transparency
- Xresources
- No-bright-on-bold
- Boxdraw
- Font zoom shortcuts
- Local colors, font, and geometry

HarfBuzz should be optional and added late. Font2 should be added only if testing demonstrates a need.

## Implementation Workflow

Use a separate worktree so the current branch and its uncommitted configuration changes remain untouched:

```sh
git worktree add -b reflow-rebuild ../st-reflow d63b9eb
cd ../st-reflow
```

Apply and commit each foundational patch independently:

```sh
git apply /path/to/st-scrollback-0.9.2.diff
git add .
git commit -m "apply scrollback patch"

git apply /path/to/st-scrollback-reflow-0.9.2.diff
git add .
git commit -m "apply scrollback reflow patch"
```

Then build from a generated configuration:

```sh
make clean
rm -f config.h
make
```

Each subsequent patch should be its own commit. Avoid a new monolithic "merge all Luke changes" commit.

## Test Plan

### Reflow correctness

- Print a paragraph longer than the terminal width.
- Resize repeatedly: `80 → 20 → 120 → 40 → 80` columns.
- Confirm soft-wrapped text rejoins when widened.
- Confirm explicit newlines never join.
- Confirm trailing spaces do not produce visible garbage.
- Confirm colors, bold, underline, reverse video, and other attributes follow their glyphs.

### Cursor behavior

- Resize with the cursor in the middle of a wrapped logical line.
- Resize with the cursor at the right margin.
- Resize immediately after a character that set wrap-next state.
- Grow and shrink terminal height around the cursor.
- Confirm command-line editing remains usable after resize.

### Wide glyphs

- Test CJK characters at the right margin.
- Test emoji and Nerd Font symbols.
- Ensure a wide glyph and its dummy cell are never split across rows.
- Repeat narrow/wide resize cycles to detect corruption.

### Scrollback

- Resize at the live bottom.
- Resize while scrolled several pages back.
- Fill history to its configured capacity before resizing.
- Confirm scrolling cannot access freed, blank, or duplicated rows.
- Document whether the viewport preserves its logical top line or its distance from the bottom.

### Alternate screen

Test at least:

- Vim
- less
- tmux
- A curses application such as htop

Resize each application repeatedly and confirm it repaints correctly. Primary-screen history must not be merged with alternate-screen contents.

### Selection and clipboard

- Select and copy hard-wrapped and soft-wrapped text.
- Confirm copied soft wraps do not gain newlines.
- Confirm hard breaks do gain newlines.
- Resize while a selection exists; clearing the selection is acceptable for the initial version if behavior is documented.

### Rendering

After each rendering patch, test:

- Box drawing at multiple font sizes
- Ligatures, if enabled
- Fallback symbols and emoji
- Cursor drawing over wide and shaped glyphs
- Dirty borders after resize and color changes

### Regression checks

- Build with a freshly generated `config.h`.
- Run under AddressSanitizer during resize stress testing if feasible.
- Exercise clear screen, reset, alternate-screen switching, and clear-history behavior.
- Run external helpers only after they have been ported to the new history model.

## Acceptance Criteria

The rebuild is ready to replace the current branch when:

- Repeated width changes preserve primary-screen text correctly.
- Hard and soft line endings remain distinguishable when copying text.
- Cursor placement remains usable after resizing.
- Wide glyphs do not corrupt adjacent rows.
- Scrollback works before and after resize.
- Alternate-screen applications repaint normally.
- The project builds from `config.def.h` without relying on a stale tracked `config.h`.
- Each non-configuration feature has a distinct commit and documented source.
- The chosen daily-use features have been tested rather than copied wholesale.

## Risks and Mitigations

### Reflow patch age and upstream differences

The available integrated reflow patch targets 0.9.2, while this repository includes 0.9.3 fixes.

**Mitigation:** apply the patch on its exact 0.9.2 base first, then bring in 0.9.3 changes. Resolve conflicts with reflow semantics in mind rather than forcing patch context onto 0.9.3.

### Patch interactions in `x.c`

Alpha, Xresources, anysize, boxdraw, Font2, and HarfBuzz overlap substantially.

**Mitigation:** apply one at a time, build after each commit, and omit patches that no longer provide enough value.

### History-dependent local tools

External pipe and normal mode assume the current physical history layout.

**Mitigation:** defer them. Port external pipe against explicit chronological history accessors. Treat normal mode as a separate enhancement.

### Fixed physical history capacity

Narrowing the terminal creates more physical rows and may evict older history.

**Mitigation:** accept and document this behavior initially. A byte-based or logical-line history store is outside this proposal's scope.

## Migration and Rollback

The current `main` branch remains intact during development. The rebuild lives on `reflow-rebuild` until accepted.

Before switching permanently:

1. Tag or branch the current build.
2. Compare key bindings and command-line behavior.
3. Copy only intentional configuration values.
4. Run both builds in daily use for a short evaluation period.
5. Merge or promote `reflow-rebuild` only after the acceptance criteria are met.

Rollback consists of returning to the preserved current branch; no in-place history rewrite is required.

## Recommendation

Proceed with the clean rebuild. Treat scrollback and reflow as the base architecture, then restore only features that earn their maintenance cost. This is less risky than modifying the current resize and history model and should produce a smaller, clearer patch stack that is easier to update in the future.
