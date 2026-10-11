# Command parsing — quoting, balanced strings, typed expansions

Condensed reference for how Kakoune parses a command line. Source:
`https://github.com/mawww/kakoune/tree/master/doc/pages/command-parsing.asciidoc`. Read this before writing any script
that contains `%`, quotes, or braces — this is the second most common trip-up.

## Basic parsing

- Commands are terminated by `;` or end-of-line.
- Words (command names and parameters) are delimited by whitespace.

## Quoted strings

A word starting with `'`, `"`, or `%X` (X = a **non-nestable** punctuation
char, e.g. `|`, `#`, `@`) is a quoted string whose delimiter is `'`, `"`, or X.

- A quoted string contains every character including whitespace.
- **Doubling the closing delimiter escapes it** — two closing delimiters render
  one literally.
- **Inside `"`, `%`-strings are processed unless `%` is doubled** (`%%`).
  Double quotes inside must also be escaped (`""`).
- No other escaping happens in quoted strings.

| Word | Content |
| --- | --- |
| `'foo'` | `foo` |
| `foo'bar'` | `foo'bar'` (verbatim) |
| `foo% \| bar \|` | `foo% \| bar \|` (verbatim) |
| `'foo''bar'` | `foo'bar` (single word) |
| `"baz"""` | `baz"` (single word) |
| `% \| foo \| \| bar \|` | `foo \| bar` (single word) |
| `"foo % \| ""bar \| %%,baz,"` | `foo "bar %,baz,` (single word) |

## Balanced strings

A word starting with `%X` where X is a **nestable** punctuation char (`(`, `[`,
`{`, `<`) is a balanced string whose closing delimiter matches the opener
(`)`, `]`, `}`, `>`).

- There is **no way to escape** the delimiters, even nested inside other strings.
- Delimiters must be balanced; a mismatched `%{` is a **parse error**.
- Braces other than the one used need not be balanced: `%{nest{ed} non[nested}`
  is valid and expands to `nest{ed} non[nested`.

| Word | Content |
| --- | --- |
| `%{foo}` | `foo` |
| `%{foo\{bar}}` | `foo\{bar}` |
| `foo%{bar}` | `foo%{bar}` (verbatim) |
| `"foo %{bar}"` | `foo bar` |
| `%{foo\{}` | **parse error** (unbalanced) |
| `%[foo\{]` | `foo\{` (different delimiters) |

## Non-quoted words

Other words are terminated by whitespace or `;`.

- A leading `\` before `%`, `'`, or `"` escapes that character and the `\` is
  discarded.
- A `\` before whitespace or `;` discards the `\` and keeps the whitespace/`;`
  as part of the word.
- Any other `\` is a literal `\`.

## Typed expansions

Quoted and balanced strings starting with `%` may have an optional alphabetic
*expansion type* between `%` and the delimiter. This defines how the content is
expanded:

- **Empty type** → content used verbatim.
- **`sh`, `reg`, `opt`, `val`, `arg`** → expanded per
  [`expansions.md`](./expansions.md).
- **Any other type** → parse error.

## Checklist — why won't it parse?

- A stray unbalanced `%{` / `%(` / `%[` / `%<` → balanced-string parse error.
- An unescaped `%` inside `"..."` → the following chars are read as a `%`-string.
  Use `%%` to emit a literal `%`.
- A doubled `""` or `''` that was meant to be two quotes → becomes a single
  literal quote.
- A doubled closing delimiter on any quoted string → becomes literal.
- Wrong expansion type (`%foo{...}`) → parse error; only `sh reg opt val arg`
  (or empty) are valid.
