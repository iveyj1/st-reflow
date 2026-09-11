# st-reflow

Personal `st` build for X, based on suckless `st` 0.9.2 plus upstream fixes through `04ce0d6`. The main goal is reliable primary-screen scrollback with width reflow.

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

`make install` also installs `st-urlhandler` and `st-copyout`. These need dmenu
and either xclip or xsel; opening URLs additionally needs xdg-open (xdg-utils).
The helpers are standalone and do not load any shared session/profile framework.
`st-copyout` selects one nonempty logical history line, not a whole command-output
region. Use keyboard copy mode for multiline selections. The URL helper recognizes
plain HTTP(S) URLs; parenthesized URL components are not supported.

Terminfo installs to `$(PREFIX)/share/terminfo`, including under DESTDIR when
staging. Override `TERMINFO_DIR` if necessary; a nonstandard prefix may require
setting TERMINFO for client applications. Verify with `infocmp st-256color`.
Uninstall leaves terminfo entries in place because other st builds may use them.

```sh
make clean
rm -f config.h
make
sudo make install
```

`config.h` is generated from `config.def.h`; this repo tracks both, so keep them synchronized. Existing terminal windows keep running the old binary until restarted.

## Defaults

- Font: `JetBrainsMono Nerd Font Mono:size=12`
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

`Alt+Escape` enters primary-screen copy mode. It consumes input locally and is disabled on the alternate screen. Resize exits copy mode.

| Key | Action |
|---|---|
| `h/j/k/l`, arrows | Move by cell/row |
| `w/e/b` | Next word / word end / previous word; punctuation is separate |
| `W/E/B` | Next WORD / WORD end / previous whitespace-delimited WORD |
| `0` / `^` | First column / first printable character of the row |
| `$` / `%` | End of row / last printable character of the row |
| `gg` / `G` | Oldest history / live cursor |
| `[count]gg`, `[count]G` | Go to physical history row `count` |
| `Ctrl+u/d` | Half page up/down |
| PageUp/PageDown | Full page up/down |
| Numeric prefix | Repeat movement commands, for example `5j`, `3w`, or `2$` |
| `v` / `V` | Characterwise / logical-line selection |
| `y` | Yank selection or current logical line to PRIMARY and CLIPBOARD |
| Escape, `q`, `i`, Enter, `Ctrl+c` | Leave copy mode |

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
