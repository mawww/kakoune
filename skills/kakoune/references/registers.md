# Registers

Condensed reference for registers. Source: `https://github.com/mawww/kakoune/tree/master/doc/pages/registers.asciidoc`.

Registers are **named lists of text** (not just text) so they interact well
with multiselection. Used for yanked text, selection locations, regex captures,
etc.

## Interacting

- `<c-r><c>` in insert/prompt mode inserts the contents of register `<c>`.
- `"<c>` in normal mode selects register `<c>`.

## Alternate names

Non-alphanumeric registers have an alphanumeric **alternate name** for use
where only identifiers are valid (e.g. in `%reg{...}`):

| Symbol | Alternate |
| --- | --- |
| `/` | `slash` |
| `"` | `dquote` |
| `*` | `star` |
| `@` | `arobase` |
| `^` | `caret` |
| ` \| ` | `pipe` |
| `%` | `percent` |
| `.` | `dot` |
| `#` | `hash` |
| `_` | `underscore` |
| `:` | `colon` |

## Default registers (normal-mode commands)

| Register | Purpose | Used by |
| --- | --- | --- |
| `"` | delete / copy / paste / replace | `c d y p <a-p> <P> R <a-R>` |
| `/` | search / regex (prompt history, last 100 commands) | `* <a-/> ? <a-?> n <a-n> N <a-N> * <a-*> s S <a-k> <a-K>` |
| `@` | macro (record) | `q Q` |
| `^` | mark | `z <a-z> Z <a-Z>` |
| ` \| ` | shell command (prompt history, last 100) | `\| <a- \| > ! <a-!>` |

## Special registers (read-only)

| Register | Contents |
| --- | --- |
| `%` | current buffer name |
| `.` | current selection contents |
| `#` | selection indices (1-based) |
| `_` | null register, always empty |
| `:` | prompt history (last 100 commands, excluding leading-space ones) |

## Integer registers `1`–`9`

Hold the grouped sub-matches of the last selection regex. Example:

```plaintext
(\w+) (\w+) (\d+) .+
```

applied to a date puts the weekday in register `1`, month in `2`, day in `3`.

## Marks (marks stored by `z` / `Z`)

Marks are lists of spans like `%val{selections_desc}`. The **first** item has
the form:

```plaintext
<buffer name>@<timestamp>@<main sel index>
```

- `<buffer name>` — `%val{buffile}` the selections relate to.
- `<timestamp>` — `%val{timestamp}` at which the selection applies.
- `<main sel index>` — 0-based index of the main selection.
