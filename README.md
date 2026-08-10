# st with scrollback reflow

This is a small, deliberately rebuilt st configuration centered on correct scrollback reflow. It replaces an older Luke Smith-derived custom tree rather than attempting to retrofit reflow into that tree's `Term.maxcol`, history, browse-mode, and external-pipe modifications.

The current daily-use candidate is branch **`reflow-skip-active-line`**. It is based on stock st 0.9.2, the official scrollback and reflow patches, and upstream fixes through `04ce0d6` (post-0.9.3).

See [PATCHES.md](PATCHES.md) for exact patch sources and [REBUILD_HISTORY.md](REBUILD_HISTORY.md) for the decisions, failed experiments, tests, and current limitations.

## Current feature set

- Primary-screen scrollback with width reflow
- Hard/soft line distinction in completed output
- Keyboard and mouse history scrolling
- Clipboard copy and paste
- Font zoom
- Anysize windows
- Focused and unfocused background opacity
- Startup Xresources loading
- No-bright-on-bold
- Boxdraw rendering
- Brief, history-free version/date splash overlay
- Dependency-free Alt-F4/process-close warning overlay
- Reflow-aware external pipe with URL and command-output helpers
- Vim-style keyboard copy mode spanning offscreen history
- Vim-compatible `CSI 3 J` and modern capability-probe handling
- Stock fontconfig fallback, without Font2 or HarfBuzz

Deliberately omitted:

- HarfBuzz/ligatures
- Explicit Font2 fallback
- Runtime Xresources reload by signal

## Active-line behavior

Whole-screen reflow and readline compete over an active shell prompt: st reflows the prompt, then readline redraws it after `SIGWINCH`. With progressive resizes, stale prompt copies can become history and later reflow into completed command output.

The current branch defaults to:

```c
int reflowactive = 0;
```

Completed output and history still reflow normally. When the original terminal child owns the foreground process group, st maps the cursor but clears its active logical line, allowing readline to repaint it without preserving stale copies.

This intentionally sacrifices an unfinished shell-owned line during resize. Set `reflowactive` to `1` in `config.def.h` to restore the standard reflow patch behavior.

## Build

Requirements include a C99 compiler, make, pkg-config, Xlib, Xft, fontconfig, and FreeType development files.

```sh
make clean
rm -f config.h
make
./st
```

`config.h` is generated from `config.def.h` and is ignored. A clean build must never depend on an old local `config.h`.

Install the binary and terminfo entry using:

```sh
sudo make install
```

The default prefix is `/usr/local`. Existing terminal processes continue running their old executable and must be restarted after installation.

## Configuration defaults

- Font: `DroidSansM Nerd Font Mono:size=15`
- Geometry: `120x42`
- Focused opacity: `0.8`
- Unfocused opacity: `0.5`
- History capacity: 2000 physical rows
- Splash: `st-reflow 0.1 · 2026-08-06`, 900 ms, dim color 8
- Colors: Gruvbox-derived dark palette

Stock fontconfig fallback is used for symbols and emoji. The DroidSansM font is a preference, not a functional dependency; change `font` in `config.def.h` on systems where it is unavailable.

### Main shortcuts

| Binding | Action |
|---|---|
| `Ctrl+Shift+C` / `Ctrl+Shift+V` | Copy / clipboard paste |
| `Ctrl+Shift+Y` | Paste primary selection |
| `Shift+Insert` | Paste primary selection |
| `Shift+PageUp/PageDown` | Scroll history by a page |
| `Alt+PageUp/PageDown` | Scroll history by a page |
| `Alt+Up/Down` | Scroll history by one line |
| `Alt+U/D` | Scroll history by a page |
| `Ctrl+Shift+PageUp/PageDown` | Increase / decrease font size |
| `Ctrl+Shift+Home` | Reset font size |
| Mouse wheel | Scroll four history rows |
| `Ctrl` + mouse wheel | Change font size |
| Middle click | Paste primary selection |
| `Alt+F4` | Close immediately when idle; require a second press when a process is running |
| `Alt+Escape` | Enter or leave keyboard copy mode |
| `Alt+l` | Choose a URL from primary-screen history and open it |
| `Alt+y` | Choose a URL from primary-screen history and copy it |
| `Alt+o` | Choose a command and copy its output |

## Keyboard copy mode

`Alt+Escape` enters a primary-screen copy mode whose cursor can move through saved history beyond the visible viewport. Copy mode consumes ordinary input instead of sending it to the pseudoterminal.

| Key | Action |
| --- | --- |
| `h/j/k/l`, arrows | Move by one displayed cell or row |
| `0`, `$`, Home, End | Move to beginning or end of displayed row |
| `gg` | Move to oldest available history |
| `G` | Move to the live terminal cursor |
| `Ctrl+u`, `Ctrl+d` | Move half a page |
| Page Up, Page Down | Move a full page |
| `v` | Toggle characterwise selection |
| `V` | Toggle logical-line selection, including soft-wrapped rows |
| `y` | Copy the selection, or the current logical line, to PRIMARY and CLIPBOARD |
| Escape, `q`, `i`, Enter, `Ctrl+c` | Leave copy mode |

Copy mode uses a configurable hollow cursor drawn independently of the child application's cursor visibility. It therefore remains visible in programs such as Pi that hide the terminal cursor and paint their own editor cursor. Copy mode is disabled on the alternate screen. Any terminal resize cancels it rather than attempting to preserve cursor and selection coordinates through reflow. Global `Alt+F4` close handling remains available while copy mode is active.

## External pipe

The external pipe exports available primary-screen history in chronological order, followed by visible screen contents. Soft-wrapped physical rows are joined; hard line endings remain newlines. Trailing unused screen rows are omitted, and alternate-screen contents are not exported.

The pipe mechanism itself has no dmenu dependency. The configured `st-urlhandler` and `st-copyout` consumers use dmenu and xclip for their selection interfaces.

## Close warning

When no descendant process is running, `Alt+F4` and a window-manager close request exit immediately. If a process is still running, st displays:

```text
process still running · Alt-F4 again to close · Esc to cancel
```

A second `Alt+F4` or close request within four seconds confirms. `Esc` cancels and is consumed; mouse or ordinary keyboard input cancels while continuing normally. The warning is an internal Xft overlay, so it has no dmenu or shell-command dependency and never enters terminal history.

On Linux, activity detection scans `/proc` for live descendants of the terminal shell, including background jobs. Other systems use the foreground terminal process group as a portable fallback.

## Xresources

Resources are loaded at startup. Supported entries include:

```text
st.font: DroidSansM Nerd Font Mono:size=15
st.alpha: 0.8
st.alphaUnfocused: 0.5
st.background: #080808
st.foreground: #ebdbb2
st.cursorColor: #add8e6
st.borderpx: 2
```

Colors `st.color0` through `st.color15` and several latency, geometry, and terminal settings are also supported; see the `resources` table in `config.def.h`.

Runtime `SIGUSR1` reload was omitted because the available patch called Xlib and allocator functions from a signal handler, which is not async-signal-safe.

## Splash overlay

The startup label is drawn directly with Xft in the lower-right corner. It is not sent through the pseudoterminal and can never enter scrollback or reflowed text. It disappears after 900 ms or immediately on keyboard/mouse input; hiding it forces a complete redraw so no pixmap remnants remain. Embedded windows (`-w`) do not show it. The close warning reuses the same rendering and monotonic-timer mechanism.

Update `splashtext` in `config.def.h` when assigning a new release version or date. Set `splashtimeout` to `0` to disable it.

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
