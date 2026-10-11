# Expansions

Condensed reference for `%` expansions. Source: `https://github.com/mawww/kakoune/tree/master/doc/pages/expansions.asciidoc`.

## Structure

Every expansion is `%` + **type** (one or more letters) + a **quoting char** +
the text up to its matching character.

- If the quoting char is nestable (`(`, `[`, `{`, `<`), the expansion ends at
  the matching close and nesting is allowed (braces must be balanced).
- Any other quoting char ends at its next occurrence and can be escaped by
  doubling.
- `{}` are the most common.

## Quoting modes

- **Double-quoted `"..."`** — groups words into one argument; `%` and `"` are
  escaped by doubling (`%%`, `""`). Expansions are processed.
- **Single-quoted `'...'`** — no expansion; `'` escaped by doubling (`''`).

Expansions process when unquoted and inside `"..."`, but **not** inside
unquoted words, single-quoted strings, `%-strings`, or nested expansions.

| Example | Result |
| --- | --- |
| `echo %val{session}` | current session ID |
| `echo x%val{session}x` | literal `x%val{session}x` |
| `echo '%val{session}'` | literal `%val{session}` |
| `echo "x%val{session}x"` | session ID surrounded by `x` |
| `echo %{%val{session}}` | literal `%val{session}` |
| `echo %sh{ echo %val{session} }` | literal `%val{session}` |

Unlike shell, Kakoune expansions don't accidentally split on whitespace — only
list-type values (list options, registers, selections) expand to multiple
words. So leaving an expansion unquoted is safe.

## Typed expansions

Only these types are valid (others are parse errors):

| Type | Expands to |
| --- | --- |
| `arg` | command argument(s) — only inside `define-command` `-params` |
| `opt` | option value in the current scope (`%opt{BOM}`) |
| `reg` | register contents (`%reg{/}`, or by alternate name) |
| `sh` | output of a shell script |
| `val` | Kakoune internal value (see below) |
| `file` | content of a file read from the host FS |
| `exp` | recursively expanded like `"..."` but `"` need not be escaped |

### `%arg{n}` / `%arg{@}`

- `%arg{n}` — argument number `n` of the current command.
- `%arg{@}` — all arguments as individual words.

## Shell expansions (`%sh{}`)

The script's stdout replaces the expansion. Kakoune blocks user input until it
finishes. Errors written to stderr are appended to the `\*debug*` buffer — check
it when debugging.

Expansions are NOT nested inside `%sh{}`, so Kakoune exports them as
**environment variables** instead:

| Expansion | Env var |
| --- | --- |
| `%arg{n}` | `$_n_` (e.g. `%arg{3}` → `$3`) |
| `%arg{@}` | `$@` |
| `%opt{x}` | `$kak_opt_x` |
| `%reg{x}` | `$kak_reg_x` (all), `$kak_main_reg_x` (main only) |
| `%val{x}` | `$kak_x` |

- Quote list values with `$kak_quoted_` prefix (e.g. `"$kak_quoted_selections"`).
- Only variables actually mentioned (even in a comment) are exported.
- `$kak_command_fifo` accepts commands executed when the fifo is closed;
  `$kak_response_fifo` returns data.

```sh
eval set -- "$kak_quoted_selections"
while [ $# -gt 0 ]; do ...; shift; done
```

## Value expansions (`%val{...}`)

Common ones (context in *italics*):

- `%val{buffile}` / `%val{bufname}` — buffer file / name (*buffer, window*).
- `%val{buf_line_count}` — line count.
- `%val{bufname}` — buffer name.
- `%val{client}` / `%val{client_pid}` / `%val{client_list}` — client info
  (*window*).
- `%val{count}` — count when a mapping was triggered.
- `%val{cursor_byte_offset}` / `%val{cursor_column}` / `%val{cursor_line}` —
  cursor position (*window*).
- `%val{error}` — error text inside `try`'s on-error command.
- `%val{history}` / `%val{history_id}` / `%val{history_since_id}` — undo history.
- `%val{hook_param}` / `%val{hook_param_capture_n}` — hook param, capture group
  (hook command only).
- `%val{modified}` — `true` if unsaved changes.
- `%val{register}` / `%val{recording_register}` — register state.
- `%val{selection}` / `%val{selections}` / `%val{selections_desc}` — selection
  content(s) (*window*).
- `%val{selection_desc}` — `a.b,c.d` range of main selection.
- `%val{selection_count}` / `%val{selection_length}` / `%val{selections_length}`.
- `%val{session}` — session name.
- `%val{source}` — path of the `.kak` file being sourced.
- `%val{timestamp}` — buffer modification timestamp (needed for spec
  highlighters).
- `%val{uncommitted_modifications}` — pending insertions/deletions.
- `%val{version}` — server version.
- `%val{window_height}` / `%val{window_width}` / `%val{window_range}` — window
  geometry (*window*).
- `%val{text}` — text entered in a `prompt` command.
- `%val{user_modes}` — unquoted list of user modes.

Values with no context listed are available everywhere. "Quoted list" follows
string-quoting rules; "unquoted list" cannot contain special chars.

## Recursive expansions (`%exp{}`)

`%exp{}` expands its content the way `"..."` does, except double quotes don't
need to be escaped.
