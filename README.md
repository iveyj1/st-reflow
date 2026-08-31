# st-reflow

Personal `st` build for X, based on suckless `st` 0.9.2 plus upstream fixes through `04ce0d6`. The main goal is reliable primary-screen scrollback with width reflow.

## Features

- Scrollback history with reflow on resize; retained output keeps hard/soft line boundaries.
- Active shell prompt resize guard: unfinished foreground-shell lines are cleared and left for readline to repaint (`reflowactive = 0`).
- Keyboard/mouse scrollback, clipboard paste/copy, font zoom, anysize, Xresources at startup.
- Geometric box drawing, no bright-on-bold, OSC 52 window operations enabled.
- Startup splash and Alt-F4 close warning drawn as X overlays, never terminal text.
- Reflow-aware external pipe helpers for URL open/copy and command-output copy.
- Vim-style keyboard copy mode that can navigate and yank offscreen scrollback.
- Terminfo `E3` / `CSI 3 J` clears saved scrollback; unsupported OSC 8 and modern Vim probes are handled quietly.
- Stock fontconfig fallback; no Font2 or HarfBuzz/ligatures.

Patch/source details are in [PATCHES.md](PATCHES.md).

## Build and install

Dependencies: C99 compiler, make, pkg-config, Xlib, Xft, fontconfig, FreeType.

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
- Opacity: `1.0` focused and unfocused
- Scrollback: 2000 physical rows
- Splash: `st-reflow 0.1 · 2026-08-06`, 2000 ms
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
| `Alt+o` | Choose command output and copy it |
| `Alt+F4` | Close; confirm if a process is running |

## Keyboard copy mode

`Alt+Escape` enters primary-screen copy mode. It consumes input locally and is disabled on the alternate screen. Resize exits copy mode.

| Key | Action |
|---|---|
| `h/j/k/l`, arrows | Move by cell/row |
| `0`, `$`, Home, End | Start/end of displayed row |
| `gg` / `G` | Oldest history / live cursor |
| `Ctrl+u/d` | Half page up/down |
| PageUp/PageDown | Full page up/down |
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

Colors `st.color0` through `st.color15` and other settings are listed in the `resources` table in `config.def.h`.
