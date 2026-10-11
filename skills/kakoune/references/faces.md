# Faces & markup

Condensed reference for faces and markup strings. Source:
`https://github.com/mawww/kakoune/tree/master/doc/pages/faces.asciidoc`.

## Face format

```plaintext
[fg][,bg[,underline]][+attributes][@base]
```

- `fg`, `bg` — foreground / background color.
- `underline` — optional underline color.
- `+attributes` — one or more attribute letters.
- `@base` — the base face to inherit from.

## Colors

- **Named**: `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`,
  `white`, `default`, etc.
- **RGB**: `rgb:RRGGBB`.
- **RGBA**: `rgba:RRGGBBAA` — the alpha component **must be > 16**, otherwise it
  is ignored.

## Attributes

| Letter | Effect | Letter | Effect |
| --- | --- | --- | --- |
| `u` | underline | `c` | crossout |
| `U` | strong underline | `r` | reverse |
| `b` | bold | `B` | bold background |
| `d` | dim / dark | `i` | italic |
| `s` | standout | `F` | foreground |
| `f` | focus | `g` | gutter |
| `a` | background | — | — |

## Markup strings

Faces are referenced inside markup strings with `{facename}`:

```plaintext
{red bold}text{default}
```

Escaping in markup:

- `\{` — a literal `{`.
- `{\}` — disables markup interpretation.

## Built-in faces (examples)

`default`, `info`, `warning`, `error`, `field`, `prompt`, `menu`,
`menu_selected`, `count`, `enter_prompt`, `leave_prompt`, `line_number`,
`current_line_number`, `search`, `nosearch`, `selection`, `visual`,
`user1` … `user9`, `mode_insert`, `mode_replace`, `mode_command`, etc.

## Usage

Faces are used in `statusline`, `prompt`, `menu` text, and completion menu
strings. Completion menu text is a markup string, so it can embed `{face}`
directives.
