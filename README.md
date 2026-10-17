# st-reflow

Personal `st` build for X, based on suckless `st` 0.9.2 plus upstream fixes through `04ce0d6`. The main goal is reliable primary-screen scrollback with width reflow.

## Change-size history

Generate the gitignored local report `scripts/cloc_by_commit.md` for per-change runtime
and support cloc counts, including upstream merges. After editing, run
`python3 scripts/update_cloc_by_commit.py` (Python 3.9+, git, cloc required).
It prints latest totals/deltas and includes pending changes separately.
Use `--check` to detect stale reports; regenerate after committing.
See AGENTS.md for the maintenance policy.

## Features

- Scrollback history with reflow on resize; retained output keeps hard/soft line boundaries.
- Active shell prompt resize guard: unfinished foreground-shell lines are cleared and left for readline to repaint (`reflowactive = 0`).
- Keyboard/mouse scrollback, clipboard paste/copy, font zoom, anysize, Xresources at startup.
- Geometric box drawing, no bright-on-bold, OSC 52 window operations enabled.
- Startup splash and Alt-F4 close warning drawn as X overlays, never terminal text.
- Reflow-aware external pipe helpers for URL open/copy and selecting a history line to copy.
- Vim-style keyboard copy mode that can navigate and yank offscreen scrollback.
- Terminfo `E3` / `CSI 3 J` clears saved scrollback; unsupported OSC 8 and modern Vim probes are handled quietly.
- Stock fontconfig fallback; no Font2 or HarfBuzz/ligatures.

Patch/source details are in [PATCHES.md](PATCHES.md).

## Build and install

Dependencies: C99 compiler, make, pkg-config, Xlib, Xft, fontconfig, FreeType, tic.

Mint packages: build-essential, pkg-config, libx11-dev, libxft-dev,
libfontconfig-dev, libfreetype-dev, ncurses-bin. Void equivalents: base-devel,
pkgconf, libX11-devel, libXft-devel, fontconfig-devel, freetype-devel, ncurses.
No distro identification, dwm, or shared session launcher is required. The
terminal works independently under other X window managers; menu/clipboard
programs below are dependencies only of the optional external helpers.

`make install` also installs `st-urlhandler` and `st-copyout`. These need dmenu
and either xclip or xsel; opening URLs additionally needs xdg-open (xdg-utils).
The helpers are standalone and do not load any shared session/profile framework.
They prefer the optional `dmenu-font` wrapper from the dmenu repository, which
reads `dmenu.font` from Xresources, and fall back to plain dmenu. Terminal font
and menu font are separate resources; both can be size 11 without a rebuild.
`st-copyout` selects one nonempty logical history line, not a whole command-output
region. Use keyboard copy mode for multiline selections. On the alternate screen,
these helpers see only the currently visible rows. The URL helper recognizes
plain HTTP(S) URLs; parenthesized URL components are not supported.

Terminfo installs to `$(PREFIX)/share/terminfo`, including under DESTDIR when
staging. Override `TERMINFO_DIR` if necessary; a nonstandard prefix may require
setting TERMINFO for client applications. Verify with `infocmp st-256color`.
Uninstall leaves terminfo entries in place because other st builds may use them.

```sh
make clean
make
sudo make install
```

Edit `config.def.h` only. `config.h` is ignored and regenerated automatically when the template changes. Existing terminal windows keep running the old binary until restarted.

## Defaults

- Font fallback: `JetBrainsMono Nerd Font Mono:size=11` (`st.font` Xresource or `st -f` overrides it)
- Geometry: `120x42`
- Focused opacity: `0.8`
- Unfocused opacity: `0.5`
- History capacity: 2000 physical rows
- Splash: `st-reflow 0.1 · CHECKIN-DATE · SHORT-HASH`, 900 ms, dim color 8
- Colors: Gruvbox-derived dark palette

## Shortcuts

| Binding | Action |
|---|---|
| `Ctrl+Shift+C` / `Ctrl+Shift+V` | Copy / clipboard paste |
| `Ctrl+Shift+Y`, `Shift+Insert`, middle click | Paste PRIMARY selection |
| `Shift+PageUp/PageDown` | Scroll one page |
| `Alt+PageUp/PageDown`, `Alt+U/D` | Scroll one page |
| `Alt+Up/Down` | Scroll one line |
| Mouse wheel | Scroll four lines |
| `Ctrl+Shift+PageUp/PageDown` | Increase / decrease font size |
| `Super+Shift+=` / `Super+Shift+-` | Increase / decrease font size |
| `Ctrl+Shift+Home` | Reset font size |
| `Ctrl` + mouse wheel | Change font size |
| `Shift` + mouse wheel | Send PageUp/PageDown to the application |
| `Alt+Escape` | Enter/leave keyboard copy mode |
| `Alt+l` / `Alt+y` | Choose URL from history and open/copy it |
| `Alt+o` | Choose a nonempty history line and copy it |
| `Alt+F4` | Close; confirm if a process is running |

## Keyboard copy mode

`Alt+Escape` enters copy mode on either screen and consumes input locally.
The normal-screen cursor stays on its text while output arrives; the view follows
it until that line expires from history. Terminal query replies do not move the view.
Pastes (including middle-click) are ignored in copy mode. Clicking mouse 1 moves
the cursor and extends an active visual selection; dragging starts characterwise
visual selection when none is active.
Relative physical-row numbers overlay the left edge, except on the copy cursor
row, which is left unobscured. Press `Ctrl+n` in copy mode to toggle numbers; they
start enabled and the setting persists for the terminal session. Numbers are
rendering only: they do not affect copied text, history, or terminal geometry.
On the alternate screen, navigation is limited to visible rows (no scrollback);
`gg` goes to the top visible row and counted `gg/G` addresses visible rows.
The application continues updating the display; copy mode does not freeze it.
Resize or switching screens exits copy mode. Every successful yank also exits
copy mode and clear the selection highlight, without changing scrollback position.
Outside copy mode, `Ctrl+Shift+C/X` clears the highlight after copying. PRIMARY
and CLIPBOARD remain available for pasting. Mouse selection alone is unchanged;
an unsuccessful copy does not exit selection mode.

| Key | Action |
|---|---|
| `h/j/k/l`, Left/Down/Up/Right | Move by cell/row |
| `w/e/b` | Next word / word end / previous word; punctuation is separate |
| `W/E/B` | Next WORD / WORD end / previous whitespace-delimited WORD |
| `0` / Home / `^` | First column / first column / first printable character of the row |
| `$` / End / `%` | End of row / end of row / last printable character of the row |
| `gg` / `G` | Oldest history / live cursor |
| `[count]gg`, `[count]G` | Go to physical history row `count` |
| `Ctrl+u` / `Ctrl+d` | Half page up/down |
| PageUp/PageDown | Full page up/down |
| Numeric prefix | Repeat movement commands, for example `5j`, `3w`, or `2$` |
| `v` / `V` | Characterwise / logical-line selection |
| `Ctrl+v` | Rectangular selection |
| `f` / `t` / `F` / `T` + character | Find forward / till forward / find backward / till backward in this logical line; counts supported |
| `;` / `,` | Repeat character-find in original / reversed direction; counts supported |
| `/` / `?` + text + Enter | Literal, case-sensitive forward / backward search over available history |
| `n` / `N` | Repeat search in original / reversed direction; counts supported |
| `Ctrl+n` | Toggle relative row numbers |
| `y` | Copy existing selection; otherwise await a motion (e.g. `3yw`, `y3W`, `yt,`, `yf:`, `yT,`, `yF:`) |
| `yy` | Copy counted whole logical lines, including the final newline; `3yy` and `y3y` are equivalent |
| `Y` | Without selection, copy through counted logical-line end; with selection, trim trailing spaces/tabs and copy |
| `Ctrl+Shift+C` | Without selection, same as `Y`; with selection, plain copy |
| `Ctrl+y` / `Ctrl+Shift+X` | Without selection, copy through counted logical-line end with trailing spaces/tabs on each line trimmed; with selection, trimmed copy |
| Escape, `q`, `i`, Enter, `Ctrl+c` | Leave copy mode; Escape first cancels a pending command/search without leaving |
| Backspace / `Ctrl+u` in search prompt | Delete one Unicode character / clear the search input |

All copies go to PRIMARY and CLIPBOARD and exit copy mode. Existing visual
selections ignore yank counts. Linewise selections (`V`, `yy`) include a final
newline; end-of-line copies (`Y` and aliases without a selection) do not add one.
Line-end counts use logical lines, joining soft wraps. Trimming preserves leading
indentation and internal whitespace, and never trims at a soft-wrap boundary.

Without a selection, bare `y` now waits for a motion: use `yy` for the old
whole-line operation. Operator and motion counts multiply (`2y3w` = `6yw`).
Character-find works standalone, in visual mode and after `y`; `f/F` include the
target and `t/T` exclude it. Failed finds/yanks do not move, copy or exit. Repeated
`t/T` skip the adjacent previous target, so `;` makes progress. Reversing with
`,` does not redefine the original direction.

Search spans retained history and live rows, or visible alternate-screen contents
only. Matches may cross soft wraps but not hard newlines. Search starts beyond
the cursor and wraps at most once per command; insufficient matches leave the
cursor unchanged. The overlay shows the prompt, wrapping and failure notices.
Enter accepts, Escape or `Ctrl+c` cancels the prompt, and empty Enter reuses the
previous pattern. Search input accepts up to 256 Unicode characters. Search is
not incremental: the cursor/visual anchor stays put until Enter. A successful
search moves to the match start and extends an active visual selection. `N`
does not redefine the original search direction. Patterns and character-find
state persist across copy-mode sessions; matches are evaluated against current
terminal contents, not a frozen snapshot.

## Xresources

Resources are loaded at startup only. Supported examples:

```text
st.font: JetBrainsMono Nerd Font Mono:size=12
st.alpha: 1.0
st.alphaUnfocused: 1.0
st.background: #080808
st.foreground: #ebdbb2
st.cursorColor: #add8e6
st.borderpx: 2
```

Colors `st.color0` through `st.color15` and several latency, geometry, and terminal settings are also supported; see the `resources` table in `config.def.h`.

Runtime `SIGUSR1` reload was omitted because the available patch called Xlib and allocator functions from a signal handler, which is not async-signal-safe.

## Splash overlay

The startup label is drawn directly with Xft in the lower-right corner. It is not sent through the pseudoterminal and can never enter scrollback or reflowed text. It disappears after 900 ms or immediately on keyboard/mouse input; hiding it forces a complete redraw so no pixmap remnants remain. Embedded windows (`-w`) do not show it. The close warning reuses the same rendering and monotonic-timer mechanism.

At build time, `buildinfo.h` is generated from the checked-out commit's date and short hash. Set `splashtimeout` to `0` to disable the splash.

## Branches

- `reflow-skip-active-line`: current daily-use candidate
- `reflow-rebuild`: standard reflow behavior before the active-line experiment
- `master`: unmodified upstream st, checked out in the original worktree

The standard branch is kept as a simple rollback and comparison point.

## Publishing to a new GitHub repository

Create an empty GitHub repository without generated README or license files. Preserve the official suckless remote as fetch-only:

```sh
git remote rename origin upstream
git remote set-url --push upstream DISABLED
git remote add github git@github.com:YOURNAME/YOUR-REPOSITORY.git
```

Publish the daily-use candidate as GitHub's `main` and retain the comparison branches:

```sh
git push -u github reflow-skip-active-line:main
git push github reflow-rebuild
git push github master:upstream-master
```

Do not push personal branches to `https://git.suckless.org/st`.

## Status

The build is suitable for extended daily-use evaluation, not yet declared final. Report regressions with:

- the foreground application
- terminal dimensions before and after resize
- whether the primary or alternate screen was active
- whether the cursor line was complete or still being edited
- a minimal command sequence that reproduces the issue
