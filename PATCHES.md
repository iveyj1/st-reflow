# st-reflow patch notes

Base: suckless `st` 0.9.2 (`d63b9eb`) with upstream fixes merged through `04ce0d6`.

## Suckless patches used

- [scrollback 0.9.2](https://st.suckless.org/patches/scrollback/st-scrollback-0.9.2.diff)  
  SHA-256: `8db63bf83df06cba12cdb02e578cb49afc789c726b4c85f66e95b372562bac7a`
- [scrollback reflow 0.9.2](https://st.suckless.org/patches/scrollback/st-scrollback-reflow-0.9.2.diff)  
  SHA-256: `005145eb973e68a9816fb659222d8efa9123828084f79a77d1746a5799acdf27`
- [bold-is-not-bright](https://st.suckless.org/patches/bold-is-not-bright/st-bold-is-not-bright-20190127-3be4cf1.diff)
- [anysize](https://st.suckless.org/patches/anysize/st-anysize-20220718-baa9357.diff)
- [alpha](https://st.suckless.org/patches/alpha/st-alpha-20240814-a0274bc.diff)
- [alpha focus highlight](https://st.suckless.org/patches/alpha_focus_highlight/st-focus-0.9.3.diff)
- [Xresources](https://st.suckless.org/patches/xresources/st-xresources-20260524-688f70a.diff), startup loading only
- [boxdraw v2](https://st.suckless.org/patches/boxdraw/st-boxdraw_v2-0.8.5.diff)

The upstream merge kept reflow's alternate-screen storage and five-argument `tclearregion` behavior where those conflicted with later upstream changes.

## Local changes from stock st

- Active-line resize policy: `reflowactive = 0` clears an unfinished foreground-shell logical line after cursor mapping so readline can repaint it without stale prompt copies entering history. Set `reflowactive = 1` for standard reflow-patch behavior.
- Reflow-aware external pipe exports primary-screen history oldest-first, appends visible content, joins soft wraps, and skips unused trailing rows.
- URL and command-output helpers use `st-urlhandler` and `st-copyout` through `Alt+l`, `Alt+y`, and `Alt+o`.
- Keyboard copy mode replaces the old normal/browse mode with local Vim-style navigation and yanking over current reflowed physical rows.
- Alt-F4/window close uses process detection plus a nonblocking Xft confirmation overlay instead of spawning dmenu.
- Startup splash is an X overlay, not terminal output.
- `CSI 3 J` clears saved history and is advertised as terminfo `E3`.
- OSC 52 is allowed (`allowwindowops = 1`). Unsupported OSC 8 and common modern terminal capability probes are recognized quietly.

## Deliberately omitted

- Font2
- HarfBuzz/ligatures
- Runtime Xresources reload by signal, because the available patch performed non-async-signal-safe Xlib/allocation work in a signal handler

Stock fontconfig fallback is used instead.
