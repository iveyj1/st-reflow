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

Synchronized-output mode (`CSI ? 2026 h/l`) is recognized and ignored rather than implemented. Modern Vim capability probes are answered or ignored without diagnostics, and `CSI 3 J` clears inaccessible scrollback as specified by xterm.

A local reflow fix handles readline-style `CR + EL` redraws after `SIGWINCH`. It detaches reflowed prompt-prefix rows before the application replaces its active line, preventing a later widening from joining the stale and replacement prompts and mapping the cursor into the duplicated text.

## Deliberately omitted

- Font2
- HarfBuzz/ligatures
- Vim-style browse mode
- External pipe and URL/output helpers
- Close-warning dialog

Stock fontconfig fallback is used. The preferred primary font is `DroidSansM Nerd Font Mono`.

## Build

Always verify that the generated configuration builds:

```sh
make clean
rm -f config.h
make
```

`config.h` and build products are local artifacts and must not be committed.
