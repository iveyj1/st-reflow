# Terminal Markdown Rendering Test

This document exercises Markdown table layout, Unicode width handling, inline styling, wrapping, and geometric box drawing in terminal renderers.

## Basic table

| Feature | Status | Notes |
| --- | --- | --- |
| Scrollback reflow | Enabled | Preserves completed logical lines across width changes |
| Keyboard copy mode | Enabled | Supports offscreen characterwise and linewise selection |
| External pipe | Enabled | Exports retained primary-screen history chronologically |
| Geometric box drawing | Enabled | Synthesizes aligned Unicode table borders |

## Alignment and numeric values

| Item | Left aligned | Centered | Right aligned |
| :--- | :--- | :---: | ---: |
| Alpha | ordinary text | active | 1,024 |
| Beta | **bold text** | pending | 65,536 |
| Gamma | *italic text* | complete | 1,048,576 |
| Delta | `inline_code()` | unknown | -42.75 |

## Wrapping stress test

| Component | Short description | Longer explanation intended to wrap in a narrow terminal |
| --- | --- | --- |
| Reflow | Width-aware history | Completed terminal output is reconstructed from soft-wrapped physical rows when the terminal width changes. |
| Copy mode | Vim-style navigation | The copy cursor can move beyond the visible viewport, and logical-line selection joins soft wraps without inserting false newlines. |
| External pipe | Plain UTF-8 export | Available history is emitted oldest-first while preserving hard line endings and joining wrapped continuations. |
| Clear history | Terminfo `E3` | Ordinary `clear` requests visible-screen and saved-history erasure, while `clear -x` preserves scrollback. |

## Unicode width

| Script or symbol | Example | Expected width behavior |
| --- | --- | --- |
| ASCII | terminal | One cell per character |
| Box drawing | ┌─┬─┐ │ ├─┼─┤ └─┴─┘ | Lines should meet cleanly at cell boundaries |
| CJK | 界 日本語 漢字 | Wide characters should occupy two terminal cells |
| Greek | α β γ λ Ω | Typically one cell each |
| Combining mark | é café | Base character and combining accent should remain together |
| Emoji | ✓ ⚠ 🚀 🔧 | Width depends on terminal Unicode policy and font fallback |
| Nerd Font |   󰆍 | Private-use glyphs should remain cell-aligned |

## Sparse and uneven cells

| Name | Value | Comment |
| --- | --- | --- |
| Empty value |  | The middle cell is intentionally empty |
| Empty comment | present |  |
| Punctuation | `[]{}()<>=+-*/` | Dense punctuation can expose spacing differences |
| Long token | `abcdefghijklmnopqrstuvwxyz0123456789` | May wrap when the table is narrow |

## Links and inline formatting

| Kind | Example | Notes |
| --- | --- | --- |
| Link | [suckless st](https://st.suckless.org/) | Pi may emit OSC 8 hyperlink delimiters |
| Strong | **important text** | Header and cell styling should not alter column width |
| Emphasis | *emphasized text* | Italic metrics must remain cell-aligned |
| Code | `printf '\033[3J'` | ANSI escapes are shown as text inside code spans |
| Mixed | **bold**, *italic*, and `code` | Visible width should ignore styling escapes |

## Box-drawing reference

```text
┌──────────────┬──────────────┬──────────────┐
│ Top left     │ Top center   │ Top right    │
├──────────────┼──────────────┼──────────────┤
│ Middle left  │ Center       │ Middle right │
├──────────────┼──────────────┼──────────────┤
│ Bottom left  │ Bottom center│ Bottom right │
└──────────────┴──────────────┴──────────────┘
```

## Checklist

- [ ] Horizontal rules are continuous.
- [ ] Vertical rules are continuous.
- [ ] Corners and junctions meet without gaps.
- [ ] Bold headers do not change column alignment.
- [ ] Wrapped cells preserve table borders.
- [ ] CJK and symbols do not shift following columns.
- [ ] Narrowing and widening st keeps completed table output coherent.
