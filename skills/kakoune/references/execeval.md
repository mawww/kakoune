# execute-keys vs evaluate-commands

Condensed reference for `execute-keys` and `evaluate-commands`. Source:
`https://github.com/mawww/kakoune/tree/master/doc/pages/execeval.asciidoc`. Read this before writing anything that runs
keys or commands.

## The core distinction

| | `execute-keys` | `evaluate-commands` |
| --- | --- | --- |
| Runs | keystrokes, as if pressed | commands, as if typed at the prompt |
| Subject to mappings? | **yes** (unless `-with-maps`) | no |
| Registers saved by default | `* " \| ^ @ :` (restored after) | none |
| Extra switches | `-with-maps`, `-with-hooks` | `-no-hooks`, `-verbatim` |

- Use **`execute-keys`** when you want to drive the editor through its normal
  key bindings. `execute-keys j` re-runs whatever `j` is mapped to. It is
  subject to mappings.
- Use **`evaluate-commands`** when you want a command to run *directly*,
  bypassing mappings. `evaluate-commands delete-anchored` runs that command
  regardless of any keymap for the keystrokes it looks like.

By default both run in the context of the current client and stop at the first
error.

## The `local` scope trap

**`evaluate-commands` inserts a `local` scope.** An `option=` command inside it
sets a *local* option, not a global one. If you need global scope, either set
the option outside the `evaluate-commands`, or wrap and target explicitly:

```kakoune
# local option (default behaviour)
evaluate-commands my-option=value

# global option — set outside evaluate-commands
set-option global my-option=value
```

## Switches for both commands

| Switch | Meaning |
| --- | --- |
| `-client <names>` | Execute for each client in the comma list; `*` iterates all clients |
| `-try-client <name>` | Execute for that client if it exists, else current context |
| `-draft` | Run in a **copy** of the context — buffer changes don't touch the user's selection/input |
| `-itersel` | Run once per selection, each with its own context (prevents selections merging) |
| `-buffer <names>` | Execute for each buffer in the comma list; `*` iterates all non-debug buffers |
| `-save-regs <regs>` | Registers to restore after (overwrites the default list) |

### Registers

- Without `-save-regs`, `execute-keys` **saves and restores** `*`, `"`, `|`,
  `^`, `*@`, `:`. `evaluate-commands` saves **none** by default.
- `-save-regs <regs>` takes a string of register names to restore after
  execution, replacing the default set.

## Switches specific to `evaluate-commands`

- `-no-hooks` — disable hook execution while running.
- `-verbatim` — don't reparse/split positional args; forward them exactly as
  given.

## Switches specific to `execute-keys`

- `-with-maps` — use a custom key mapping instead of the built-in one.
- `-with-hooks` — trigger existing hooks while executing keys.

## Common patterns

```kakoune
# Touch the buffer without disturbing the user's selection:
execute-keys -draft { ... }

# Run once per selection so selections don't merge:
evaluate-commands -itersel { ... }

# Bypass mappings and hooks for a direct command:
evaluate-commands -no-hooks { delete-anchored }
```

## When a choice breaks

- A script that "works" in one context but silently misbehaves in another is
  almost always the wrong command here — e.g. using `execute-keys` when a direct
  command was needed (it fired a keymap), or using `evaluate-commands` when a
  global option was needed (it set a local one).
- A hook that fires unexpectedly inside `execute-keys`: wrap in
  `execute-keys -with-maps` / `-with-hooks` deliberately, or use `evaluate-commands`.
