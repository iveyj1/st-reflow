# Rebuild history and current status

## Why this tree exists

The previous custom tree lives separately in `../st`. It combined Luke-style scrollback, a circular history buffer, external pipes, URL helpers, Vim-style terminal browsing, selection changes, anysize, alpha, Xresources, Font2, boxdraw, HarfBuzz, and local close behavior.

Its resize implementation retained the widest allocation ever seen through `Term.maxcol`. Hidden columns were preserved, but soft-wrapped logical lines were not reconstructed. Adding reflow there would have required reconciling every history-dependent feature at once.

The rebuild therefore began from stock st and treated scrollback reflow as the data-model foundation. The old tree was not modified or merged wholesale.

## Foundation

The initial base was stock st 0.9.2 at `d63b9eb`.

The official patches were applied in their required order:

1. `st-scrollback-0.9.2.diff`
2. `st-scrollback-reflow-0.9.2.diff`

They were committed independently as `e0f95a1` and `342798b`. Exact hashes and URLs are recorded in [PATCHES.md](PATCHES.md).

Upstream changes through `688f70a` were merged next. The merge retained the reflow patch's alternate-screen storage and five-argument `tclearregion` behavior while taking upstream parser, embedding, clear-screen, OSC, and safety fixes. The later async-signal-safe `sigchld` fix at `04ce0d6` was merged separately.

This produced a buildable st 0.9.3-derived reflow foundation without `Term.maxcol`.

## Features added incrementally

Each functional addition was kept in a separate commit and built before continuing:

1. Bold-is-not-bright
2. Local font, colors, geometry, clipboard, zoom, and scroll bindings
3. Recognition (but not implementation) of synchronized-output mode
4. Anysize
5. Alpha transparency
6. Focus-dependent opacity
7. Startup Xresources loading
8. Boxdraw
9. `CSI 3 J` erase-saved-lines handling
10. Modern Vim terminal capability handling
11. A temporary Xft version/date splash that never enters terminal history
12. A dependency-free Alt-F4/process-close warning overlay
13. Reflow-aware chronological history export and external-pipe helpers
14. A focused Vim-style keyboard copy mode

Font2 and HarfBuzz were intentionally omitted. DroidSansM is the preferred primary font, while stock fontconfig fallback handles other glyphs.

The Xresources patch's runtime signal reload was removed because it performed Xlib, allocation, resize, and drawing work directly inside a signal handler.

## Vim diagnostics

Vim initially produced messages such as:

```text
erresc: unknown csi ESC[3J
```

`CSI 3 J` now clears saved history using the reflow history counters rather than the old custom tree's physical-history implementation.

Newer Vim versions also probe DECRQM, kitty keyboard flags, and modifyOtherKeys. Unsupported probes are now answered or ignored quietly instead of being printed as parser errors. Vim startup and exit were smoke-tested after these changes.

## Readline and prompt reflow investigation

### Original symptoms

Progressively narrowing and widening a Bash prompt caused:

- duplicated prompt fragments
- cursor placement inside stale text
- typed characters overwriting old prompt characters
- stale prompts joining adjacent `ls` output after later reflows

A reliable reproduction was:

```sh
cd ~
clear
ls -la a*
```

Then resize progressively in approximately four-column steps. The worst corruption appeared when the prompt occupied three physical rows.

### Colored-prompt issue

Bash 5.3/readline was also observed emitting backspaces after redrawing a colored prompt. The count matched the final SGR reset sequence in `PS1`. A plain prompt avoided that separate cursor-offset symptom. No terminal-side workaround for legitimate backspaces was retained.

### Rejected approaches

Several experiments were deliberately discarded:

- Detaching soft-wrap links after a post-resize carriage return removed duplicates but converted soft boundaries into hard line breaks. It was committed temporarily and then reverted.
- Ignoring `SIGWINCH` in Bash prevented duplicate redraws but left readline using stale dimensions, breaking editing after resize.
- A no-op Bash `WINCH` trap did not suppress readline's own redraw behavior.
- Preserving active physical rows without reflow prevented accumulation but produced incorrect cursor and editing behavior after width changes.

Debug logging, synthetic X11 resize helpers, temporary inputrc/bashrc files, and terminal dumps used during these tests were not retained in the repository.

### Current active-line solution

The standard patch reflows the cursor's active logical line before `TIOCSWINSZ` causes readline to repaint it. The repaint then becomes additional terminal content, and a later resize treats old and new prompts as one logical body.

Branch `reflow-skip-active-line` records the mapped range of the cursor's logical line. After normal reflow, when the original terminal child owns the foreground process group and `reflowactive == 0`, that mapped range is cleared. The cursor remains at its reflowed location, and readline paints the current prompt into empty cells.

Completed lines and history still use normal reflow. Alternate-screen applications are unaffected. The tradeoff is intentional: an unfinished shell-owned line may disappear if its application does not repaint after `SIGWINCH`.

Set this in `config.def.h` to compare with the unmodified patch behavior:

```c
int reflowactive = 1;
```

The comparison branch `reflow-rebuild` remains at the point before this experiment.

## Testing performed

Automated and manual work has included:

- clean builds from a newly generated `config.h`
- normal warning-enabled builds
- X11 startup and exit smoke tests
- repeated wide/narrow X11 resize loops
- progressive four-column resize sequences
- the `clear; ls -la a*` prompt-corruption reproducer
- colored and plain Bash prompts
- Emacs and vi readline modes
- cursor-position reports using `CSI 6 n`
- CJK, emoji, styled text, and boxdraw output
- Vim startup/exit and capability probes
- `CSI 3 J` handling

AddressSanitizer/UBSan testing was attempted but the machine lacked the ASan runtime (`libasan`). No sanitizer result is claimed.

## Splash overlay

The inconspicuous startup label is an Xft overlay on the X backing pixmap, not terminal output. It is drawn in the lower-right corner using a dim palette color, expires through the monotonic event-loop timer, and is dismissed by keyboard or mouse input. Dismissal forces a full terminal redraw to remove the overlay cleanly. The version/date string is maintained explicitly in `config.def.h` for reproducible builds.

## Keyboard copy mode

The old normal mode stored a cursor in displayed physical rows and called selection functions directly. The replacement keeps that intentionally small command set but accounts for the reflow tree's scroll offset when creating and extending selections. It supports offscreen characterwise and logical-line yanks, copies to both PRIMARY and CLIPBOARD, refuses alternate-screen entry, and cancels on resize rather than retaining invalid physical coordinates. Its hollow Xft cursor is separate from the normal terminal cursor, so an application cannot hide it with `CSI ? 25 l`; this is required for Pi and other TUIs that paint a fake editor cursor.

## External pipe

The old external pipe traversed every slot through `TLINE_HIST`, which was tied to the former circular-history layout. The replacement walks the reflow tree's available coordinates from `-term.histf` through the visible primary screen using `TLINEABS`. UTF-8 glyphs are emitted oldest-first, soft wraps are joined, and unused rows below the final visible text are omitted. Export was verified across width reflow, wide glyphs, and saved-history rollover into the viewport.

## Close warning

The original tree launched dmenu synchronously from `quit()`. The rebuild restores its Linux `/proc` descendant detection with a foreground-process-group fallback, but replaces dmenu with a nonblocking Xft overlay. Idle terminals close immediately. A busy terminal requires the close action twice within four seconds; `Esc` cancels. Alt-F4 and `WM_DELETE` use the same path.

## Known limitations and risks

- Copy-mode `h/j/k/l` and `0/$` operate on displayed physical rows; logical-line selection uses hard/soft wrap metadata.
- Copy mode intentionally exits on any resize.
- Linux descendant detection intentionally warns for background jobs that remain children of the shell.
- `reflowactive = 0` may clear an unfinished line owned by the original terminal child.
- History capacity is 2000 physical rows; narrowing can consume capacity and evict older content.
- Synchronized-output mode is ignored, not implemented.
- Xresources are loaded only at startup.
- Selection survival across resize is not guaranteed.
- Font appearance and fallback are installation-dependent.
- The active-line behavior is local code, not part of the official reflow patch.

## Current repository state

Recommended daily-use branch:

```text
reflow-skip-active-line
```

Rollback/comparison branch:

```text
reflow-rebuild
```

The installed `/usr/local/bin/st` may differ from the worktree binary. Check running processes and paths when testing:

```sh
readlink /proc/$(pgrep -n st)/exe
```

Build and run the worktree directly when comparing behavior:

```sh
make clean
rm -f config.h
make
./st -T reflow-test
```

Install only after confirming the intended branch is checked out:

```sh
git branch --show-current
sudo make install
```

Existing terminal processes must be restarted after installation.
