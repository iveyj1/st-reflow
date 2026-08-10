# Patch stack

The rebuild starts at stock st 0.9.2 (`d63b9eb`). Functional changes are kept in separate commits and were obtained from the official st patch site.

## Foundation

1. [scrollback 0.9.2](https://st.suckless.org/patches/scrollback/st-scrollback-0.9.2.diff)  
   SHA-256: `8db63bf83df06cba12cdb02e578cb49afc789c726b4c85f66e95b372562bac7a`
2. [scrollback reflow 0.9.2](https://st.suckless.org/patches/scrollback/st-scrollback-reflow-0.9.2.diff)  
   SHA-256: `005145eb973e68a9816fb659222d8efa9123828084f79a77d1746a5799acdf27`
3. Upstream st through `688f70a`, merged after the reflow patches.
4. Post-0.9.3 upstream fix through `04ce0d6`, merged separately.

The two conflicts in the first upstream merge retained the reflow patch's alternate-screen storage and five-argument `tclearregion` semantics while incorporating the upstream fixes.

## Additional patches

- [bold-is-not-bright](https://st.suckless.org/patches/bold-is-not-bright/st-bold-is-not-bright-20190127-3be4cf1.diff)
- [anysize](https://st.suckless.org/patches/anysize/st-anysize-20220718-baa9357.diff)
- [alpha](https://st.suckless.org/patches/alpha/st-alpha-20240814-a0274bc.diff), ported to the configured tree
- [alpha focus highlight](https://st.suckless.org/patches/alpha_focus_highlight/st-focus-0.9.3.diff), adapted to change opacity without replacing the reflow or color model
- [Xresources](https://st.suckless.org/patches/xresources/st-xresources-20260524-688f70a.diff), adapted for the local color indexes and opacity settings
- [boxdraw v2](https://st.suckless.org/patches/boxdraw/st-boxdraw_v2-0.8.5.diff), ported with an attribute bit that does not conflict with reflow's attributes

Xresources are loaded at startup. The patch's `SIGUSR1` runtime reload was deliberately omitted because its signal handler called Xlib and allocator functions that are not async-signal-safe.

Synchronized-output mode (`CSI ? 2026 h/l`) is recognized and ignored rather than implemented. Modern Vim capability probes are answered or ignored without diagnostics, unsupported OSC 8 hyperlink delimiters are recognized quietly, and `CSI 3 J` clears inaccessible scrollback as specified by xterm.

The version/date splash is local X frontend code rather than a patch from suckless.org. It draws only on the backing pixmap, uses the monotonic event-loop timer, and forces a full redraw when hidden so it cannot affect terminal history or leave stale pixels.

The Alt-F4 close warning reuses the original tree's Linux `/proc` descendant detection and portable foreground-process-group fallback. Its old synchronous dmenu command was replaced by a nonblocking Xft overlay with double-close confirmation, Escape cancellation, and a four-second timeout.

The external pipe is a local, reflow-aware replacement for the old `TLINE_HIST` traversal. It enumerates only available history from oldest to newest, appends the visible primary screen, joins soft wraps, and omits unused trailing rows. The URL and command-output bindings use the existing `st-urlhandler` and `st-copyout` scripts.

Keyboard copy mode is a focused local successor to the old normal mode. Its cursor navigates the current reflowed physical rows, while visual and linewise yanks use the existing scrollback-aware selection machinery. A dedicated hollow cursor ignores application-controlled terminal-cursor hiding without modifying terminal cells. Width or height changes cancel the mode so physical coordinates are never retained across reflow.

## Active-line repaint experiment

The `reflow-skip-active-line` branch defaults `reflowactive` to `0`. Completed output and history reflow normally, but when the original terminal child owns the foreground process group, the cursor-containing logical line is cleared after its cursor has been mapped. Readline then repaints that line after `SIGWINCH` without stale prompt copies entering history. Set `reflowactive` to `1` for the standard patch behavior.

This deliberately trades preservation of an uncompleted foreground-shell line for reliable interactive prompt redraw. Full-screen child applications have their own foreground process groups and retain the standard alternate-screen behavior.

## Deliberately omitted

- Font2
- HarfBuzz/ligatures

Stock fontconfig fallback is used. The preferred primary font is `DroidSansM Nerd Font Mono`.

## Build

Always verify that the generated configuration builds:

```sh
make clean
rm -f config.h
make
```

`config.h` and build products are local artifacts and must not be committed.
