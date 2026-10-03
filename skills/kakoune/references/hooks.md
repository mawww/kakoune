# Hooks

Condensed reference for `hook`. Source: `https://github.com/mawww/kakoune/tree/master/doc/pages/hooks.asciidoc`.

## Syntax

```kakoune
hook [<switches>] <scope> <hook_name> <filtering_regex> <commands>
```

- `<scope>`: `global`, `buffer`, `window`.
- `<filtering_regex>`: the hook fires when this regex matches the hook's
  parameter (e.g. a filetype name for `BufSetOption`).
- `<commands>`: runs when it matches.

## Auto-indent example (InsertChar)

`InsertChar` fires immediately **after** a character is inserted in insert mode.
Preserve previous-line indentation:

```kakoune
:hook InsertChar \n %{ exec k<a-x> s^\h+<ret>y j<a-h>P }
```

Problems with the naive version:

- If phase 2 selects nothing it errors and leaves the user's selection on the
  previous line.
- The error is caught by the hook machinery and written to `\*debug*`, even when
  the "error" is expected (no `{`/`(` at end of previous line).

Fixes:

```kakoune
# draft: run in a copy so the user's selections are untouched
:hook InsertChar \n %{ exec -draft k<a-x> s^\h+<ret>y j<a-h>P }

# wrap in try[] so expected errors don't propagate into the hook machinery
:hook InsertChar \n %[ try %[ exec -draft k<a-x> <a-k>[{(]\h*$<ret> j<a-gt> ] ]
```

## Draft-context hooks

Some hooks run in a "draft context" where modifications to selections or input
state are **discarded** (they don't affect the user). These are
`WinCreate`, `WinClose`, `WinResize`, `WinDisplay`, `WinSetOption`. For these
hooks, use `-always` to trigger even when the draft would discard the change,
and `-no-hooks` to prevent re-entrancy.

## Env vars available inside hooks

- `kak_hook_param` — the full parameter string of the executing hook.
- `kak_hook_param_capture_N` — text captured by capture group N of the hook's
  filter regex (if it used capture groups).

## Disabling hooks

- Prefix a command with `\` to disable hooks for that command.
- `disabled_hooks` option: regex of hook names to disable.
- `-no-hooks` switch on `execute-keys` / `evaluate-commands`.

## The BufCloseFifo cleanup pattern

Combine a FIFO buffer with a hook to auto-remove the temp pipe:

```kakoune
echo "edit! -fifo ${output} *buffer-name*
      hook buffer BufClose .* %{ nop %sh{ rm -r $(dirname ${output})} }"
```

Use `-always` on the hook so cleanup runs even if a draft would otherwise drop it.

## Full hook name list

The exhaustive set lives in the full doc (`hooks.asciidoc`, "Hook names"
section). Common ones: `BufCreate`, `BufClose`, `BufSetOption`, `BufWritePre`,
`BufWritePost`, `InsertChar`, `InsertLeave`, `InsertEnter`, `ModeChange`,
`WinCreate`, `WinClose`, `WinSetOption`, `ClientAttach`, `ClientDetach`,
`UIEnter`, `UILeave`.
