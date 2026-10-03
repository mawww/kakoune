# Options

Condensed reference for declaring and setting options. Source:
`https://github.com/mawww/kakoune/tree/master/doc/pages/options.asciidoc`.

## Declare

```kakoune
declare-option [-hidden] <type> <name>
```

- `-hidden` — don't require a `-docstring`; not shown in completion.
- `<type>`: one of `int`, `bool`, `str`, `regex`, `coord`, `<type>-list`,
  `range-specs`, `line-specs`, `completions`, `enum`, `flags`,
  `<type>-to-<type>-map`.
- Only these types are **decl-able** at runtime: `int-list`, `str-list`,
  `str-to-str-map`. Others must be declared at load time.

## Set

```kakoune
set-option [-add | -remove] <scope> <name> <value>
```

- `<scope>`: `global`, `buffer`, `window`, `local`.
- `-add` / `-remove` — append/element-remove for list-typed options.

## Reading current values

Use `%opt{}` in expansions: `%opt{BOM}` → `utf8` or `none`.

## Builtin options (examples)

`tabstop`, `indentwidth`, `indentmode`, `scrolloff`, `filetype`,
`completers`, `readonly`, `BOM`, `modifiable`, `fileformat`, `encoding`,
`prompt`, `statusline`, `commentstyle`, `commenttoken`.

## Completion values

Completion options (type `completions`) carry entries with the format:

```plaintext
line.column[+length]@timestamp candidate|select|menu
```

- `line`/`column` are 1-based; `+length` is the completion length; `@timestamp`
  is the buffer timestamp the entry is valid for.
- `candidate` is the text, `select` is the command run on selection, `menu` is
  the (markup) description.

## Value types detail

- **`<type>-list`** — e.g. `int-list`, `str-list`.
- **`range-specs`** — lists of ranges, used by `ranges`/`replace-ranges`
  highlighters.
- **`line-specs`** — lists of line specs, used by `flag-lines` highlighters.
- **`enum`** — one of a fixed set of string values.
- **`flags`** — a set of named boolean flags.
- **`<type>-to-<type>-map`** — e.g. `str-to-str-map`.
