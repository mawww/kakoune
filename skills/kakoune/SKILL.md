---
name: kakoune
description: |
  Write and validate Kakoune scripts (.kak) that extend the editor with commands, options, hooks, highlighters, and mappings. Use when asked to create a Kakoune script, add a command or option, wire up a hook, build a completion or a FIFO-backed async tool, or debug why a script's keystrokes/commands/quoting misbehave. Ground every claim in the bundled resources/ docs; the two things that break people are execute-keys vs evaluate-commands and command quoting.
---

# Kakoune scripts

Write Kakoune scripts (`.kak` files) that extend the editor: define commands and
options, bind keys, run hooks, paint highlighters, and shell out to external
tools. The skill's job is to produce a script that is **correct under Kakoune's
command model** and **validated in a real `kak -ui json` instance** before it
is handed over.

This is an authoring + validation skill, not a reference dump. The authoritative
Kakoune documentation lives in `https://github.com/mawww/kakoune/blob/master/doc/`
(asciidoc). Read the matching reference file before writing anything that touches
it, and run the bundled validation script (`scripts/kak-validate.sh`) against
your script before declaring it done.

## Why

- Kakoune is a *modal* editor with a command model that differs sharply from
  vim-like editors: `execute-keys` runs keystrokes (subject to mappings) while
  `evaluate-commands` runs commands directly (and injects a `local` scope).
  Getting this wrong produces scripts that "work" in one context and silently
  misbehave in another.
- Command parsing (quoting, balanced strings, typed expansions) is subtle: a
  stray `%`, an unbalanced `%{`, or a doubled delimiter can change what your
  script actually runs.
- A script that only looks right on paper fails in practice. Validating it in
  `kak -ui json` catches scope, quoting, and command-resolution errors before
  they reach the user.

## When to use

Use this when the user wants to:

- create a new Kakoune script or add a command / option / keymap to an existing
  one,
- wire up a `hook` (for example react to a file type or a character being
  inserted),
- paint a `highlighter` (regex, column, number-lines, etc.),
- build a completion (`completers` option + candidate format) or an
  async/FIFO-backed tool,
- **debug** why a script's keystrokes, commands, or quoting misbehave — here the
  two trip-ups below are almost always the cause.

## Prerequisites

- `kak` reachable on `PATH`. Confirm with `kak -version` (version switch is
  `-version`, not `--version`). If it is missing, stop and tell the user — do
  not fabricate a validation run.
- For scripts that shell out, the external tool it depends on must be available.
  Keep dependencies minimal and expected (see `./references/scripting.md`).

## The workflow — author, then validate

1. **Scope the task.** Decide what the script adds: a command (`define-command`,
   hyphen-separated name), an option (`declare-option`, underscore-separated
   name), a mapping (`map`), a hook (`:hook`), a highlighter
   (`add-highlighter`), or a shell bridge (`%sh{}`). Read the matching reference
   file first. See [References](#references) for the index.
2. **Write the script** following the conventions in
   [Conventions](#conventions). Prefix every option/command with the script
   name; give non-hidden commands and options a `-docstring`; keep shell code
   POSIX-portable.
3. **Validate in headless Kakoune.** Run `scripts/kak-validate.sh` (see
   [Validation](#validation)). It loads your script in `kak -ui json` and
   reports command-resolution, scope, and syntax problems. Fix anything it finds
   and re-run until it is clean.
4. **Report.** Show the final script and what the validation run confirmed. If
   `kak` is unavailable, say so explicitly rather than claiming success.

## Core mental model — the two trip-ups

These two areas cause the vast majority of broken Kakoune scripts. Internalize
them; they are also the most likely cause when debugging.

### `execute-keys` vs `evaluate-commands`

| | `execute-keys` | `evaluate-commands` |
| --- | --- | --- |
| Runs | keystrokes, as if pressed | commands, as if typed at the prompt |
| Subject to mappings? | **yes** (unless `-with-maps`) | no |
| Registers saved by default | `* " | ^ @ :` (restored after) |
| Extra switches | `-with-maps`, `-with-hooks` | `-no-hooks`, `-verbatim` |

- Use `execute-keys` when you need to drive the editor through its normal key
  bindings (e.g. `execute-keys j` to move down). It re-runs whatever those keys
  are mapped to.
- Use `evaluate-commands` when you want a command to run *directly*, bypassing
  mappings (e.g. `evaluate-commands delete-anchored`).
- **`evaluate-commands` inserts a `local` scope.** An `option=` command inside it
  sets a *local* option, not a global one. Wrap the body in `evaluate-commands -no-hooks { ... }` and set options explicitly if you need global scope.
- `-draft` runs in a copy of the context so you can touch the buffer without
  disturbing the user's selections; `-itersel` runs once per selection to avoid
  selections merging. See `./references/execeval.md`.

### Command parsing — quoting, balanced strings, expansions

- Commands end at `;` or end-of-line; words split on whitespace.
- Quoted strings start with `'`, `"`, or `%X` (X = a **non-nestable**
  punctuation char). Doubling the closing delimiter escapes it. Inside `"`,
  `%`-strings are processed unless `%` is doubled.
- **Balanced strings** start with `%X` where X is `(`, `[`, `{`, or `<`; the
  closing delimiter matches, and they are *nestable* — there is no way to escape
  the delimiters. A mismatched `%{` is a parse error.
- **Typed expansions** `%sh`, `%reg`, `%opt`, `%val`, `%arg` expand the string's
  content (shell, register, option, value, argument). Any other expansion type
  is a parse error.
- A stray unbalanced `%{`, an unescaped `%` inside `"`, or a doubled `""` that
  turned into a literal `"` are the usual suspects when something does not parse
  as expected. See `./references/command-parsing.md`.

## References

The `references/` directory holds the condensed reference for Kakoune docs. Read
the matching one before writing code that touches it; do not rely on memory.

| Concern | Resource | Reference |
| --- | --- | --- |
| `execute-keys` / `evaluate-commands`, switches, `local` scope | `./references/execeval.md` | the primary trip-up |
| Quoting, balanced strings, typed expansions | `./references/command-parsing.md` | the second trip-up |
| Script conventions, POSIX shell, `-docstring`, deps | `./references/scripting.md` | conventions checklist |
| `map` / `unmap`, key names, modes, `-atomic` | `./references/mapping.md` | key bindings |
| `declare-option`, option scopes, `set-option` | `./references/options.md` | options |
| Hooks, `InsertChar`, draft contexts, `try` | `./references/hooks.md` | reactive behavior |
| `add-highlighter`, scopes, regions, passes | `./references/highlighters.md` | painting |
| Registers, save/restore | `./references/registers.md` | register handling |
| Faces, markup, colors, attributes | `./references/faces.md` | UI styling |
| `%sh{}`, `kak_session`, FIFOs, `eval -client`, completers | `https://github.com/mawww/kakoune/tree/master/doc/interfacing.asciidoc` | external tools + async |
| Buffer scopes: scratch, debug, FIFO buffers | `https://github.com/mawww/kakoune/tree/master/doc/pages/buffers.asciidoc` | scratch/FIFO buffers |
| Expansions (`%val{}`, typed expansions) | `./references/expansions.md` | value expansion |
| Modes and key descriptions | `https://github.com/mawww/kakoune/tree/master/doc/pages/modes.asciidoc` / `https://github.com/mawww/kakoune/tree/master/doc/pages/keys.asciidoc` | modes |

## Validation

`scripts/kak-validate.sh` loads a script in `kak -ui json` and reports
problems without a display. `-ui json` is Kakoune's headless mode — it runs the
server with no interactive UI, so scripts load and commands execute purely for
validation. Usage:

```bash
# Validate a script file (loads it, then exits).
scripts/kak-validate.sh path/to/my.kak

# Validate a script AND run a specific command in the same headless session
# to exercise it: provide the command as a second argument.
scripts/kak-validate.sh path/to/my.kak "my-command arg"

# Require the script to contain specific Kakoune constructs before it can pass.
# Repeatable; the script must contain every supplied token or it fails.
scripts/kak-validate.sh --require define-command --require execute-keys path/to/my.kak
```

It returns a non-zero exit code if Kakoune reports a load error or the command
fails, and echoes the Kakoune output so you can see where it broke. Use it as the
last gate before handing over a script. If `kak` is not on `PATH`, the script
prints a clear message and exits non-zero — do not skip this.

### Two structural gates

Beyond loading the script in Kakoune, the validator applies two gates that catch
the mistakes that actually reach users:

1. **Kakoune-DSL gate.** The output must look like Kakoune code — it has to
   contain at least one Kakoune keyword (`define-command`, `define-key`, `hook`,
   `add-highlighter`, `declare-option`, `execute-keys`, `evaluate-commands`,
   `set-option`, `%sh{`, or `%val{`). Pure shell or prose fails here, so a model
   that answers the prompt in the wrong language is caught rather than passing by
   accident.
2. **`--require TOKEN` gate.** Supply the constructs the task demands; the script
   must contain each one or it fails. This is the caller's contract, not baked-in
   ground truth — e.g. `--require define-command` rejects a script that used
   `define-key` instead, and `--require mytool-fix` rejects a "fix" that renamed
   the command or kept the broken one. Pass tokens with `--require TOKEN`
   (repeatable).

The validator stays **generic**: it does not know the expected answer, only that
the script loads, runs, and contains what you asked for. Pair it with the eval
fixtures in `evals/` when you want to check a specific task end to end.

### Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Script loaded and (if a command was given) ran cleanly. |
| `1` | Load/parse/runtime error, `kak` hang, wrong construct, a required `--require` token missing, or output that is not recognisable Kakoune code. |
| `2` | Usage error (bad flags) or `kak` not on `PATH`. |

## Conventions

Follow these (from `writing_scripts.asciidoc` and Kakoune's own examples):

- **Prefix** every option and command with the script name (or a one-word
  description of its purpose). Options use `_` as a separator; commands use `-`.
- **`-docstring`** every non-hidden command and option, so completion shows its
  purpose.
- **Minimal, expected dependencies.** A script is an API to external software,
  not a tool itself — keep `dependencies` small.
- **POSIX shell** inside `%sh{}`: `printf` over `echo`, `${var##*/}` over
  `basename`, `[` over `[[`, `>/dev/null 2>&1` over `&>`, `expr` over `=~`.
- **Background processes** must detach cleanly: `{ command } </dev/null >/dev/null 2>&1 &`.
- **Name options/commands consistently** and keep shell code defensive at
  boundaries (validate user input / external output, since shell is a supported
  expansion target).
- Follow the style of the bundled `examples/*.kak` scripts — they are the
  reference for real-world structure (`git.kak`, `clang.kak`, `ctags.kak`,
  `fifo.kak`, `spell.kak`).
