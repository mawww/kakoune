# Mapping

Condensed reference for `map` / `unmap`. Source: `https://github.com/mawww/kakoune/tree/master/doc/pages/mapping.asciidoc`.

## Declare

```kakoune
map <scope> <keys> <commands>
```

- `<scope>`: `insert`, `normal`, `prompt`, `user`, `goto`, `view`, `object`.
- `<keys>`: the key sequence.
- `<commands>`: runs when the keys are pressed.

```kakoune
unmap <scope> <keys>
```

## Switches

- `-atomic` — a count repeats the whole mapping, not just the last command.
- `-docstring "<text>"` — completion description (required for non-hidden maps).
- `-script` — mark as a script-defined mapping (affects unmap scoping).

## Key names

Use named forms for special keys:

| Key | Name | Key | Name |
| --- | --- | --- | --- |
| Ctrl-x | `<c-x>` | Alt-x | `<a-x>` |
| Shift-x | `<s-x>` | Literal x | `<x>` |
| Ctrl-Alt-x | `<c-a-x>` | `<` | `<lt>` |
| `>` | `<gt>` | `+` | `<plus>` |
| `-` | `<minus>` | Return | `<ret>` |
| Space | `<space>` | Tab | `<tab>` |
| Esc | `<esc>` | `;` | `<semicolon>` |
| `%` | `<percent>` | `"` | `<quote>` |
| `""` | `<dquote>` | F1–F12 | `<F1>` … `<F12>` |

## Constraints

- Cannot remap `<c-c>` or `<c-g>`.
- The `s-` (shift) modifier only applies to ASCII letters.
- A count or register is forwarded into the mapping body via
  `%val{count}` / `%val{register}`.

## Example

```kakoune
map normal <c-j> <a-j>        # Ctrl-J moves the line down
map insert <ret> <a-;>        # example scoped to insert mode
```
