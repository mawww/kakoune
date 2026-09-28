# Kakoune

> Write Kakoune scripts (`.kak`) that are correct under the editor's command model, then prove it in a headless `kak -ui json` instance before they ship.

## What this is

This skill turns authoring of Kakoune scripts into a two-phase job: **author**, then **validate**. Kakoune is a modal editor whose command model diverges sharply from vim-like editors — `execute-keys` runs keystrokes (subject to mappings) while `evaluate-commands` runs commands directly and injects a `local` scope. Command parsing (quoting, balanced strings, typed `%`-expansions) is equally subtle: a stray `%`, an unbalanced `%{`, or a doubled delimiter silently changes what the script runs. A script that looks right on paper fails in practice.

So this skill does not just produce a `.kak` file. It grounds every claim in the bundled `references/` docs and gates the output through `scripts/kak-validate.sh`, which loads the script in Kakoune's headless `-ui json` server and reports command-resolution, scope, quoting, and syntax errors. Nothing is declared done until the validator is clean.

This is an **authoring + validation** skill, not a reference dump. The authoritative Kakoune documentation lives upstream in `https://github.com/mawww/kakoune/blob/master/doc/`. Read the matching reference before touching a subsystem; do not rely on memory.

## When to use it

Use this skill when asked to create a Kakoune script or add to an existing one: a command, an option, a keymap, a hook, a highlighter, a completer, or a shell bridge (`%sh{}`). It is also the right tool for **debugging** a script whose keystrokes, commands, or quoting misbehave — the two trip-ups below cause the vast majority of such failures.

A dedicated **kakoune-expert** subagent (`agents/kakoune-expert.md`) runs the same author → validate workflow end to end. Trigger it to get a script written *and* validated without working through the steps yourself.

## Prerequisites

- `kak` on `PATH`. Confirm with `kak -version` (the flag is `-version`, not `--version`). If it is missing, stop and report it — do not fabricate a validation run.
- For scripts that shell out, the external tool they depend on must be available. Keep dependencies minimal and expected (`git.kak` → `git`, `clang.kak` → `clang`).

## Usage

The skill is invoked by the name `kakoune`. It then follows this workflow:

1. **Scope.** Decide what the script adds — a command (`define-command`, hyphen name), an option (`declare-option`, underscore name), a mapping, a hook, a highlighter, or a shell bridge. Read the matching `references/` file first.
2. **Write.** Follow the conventions below. Prefix every option/command with the script name; give non-hidden commands and options a `-docstring`; keep `%sh{}` code POSIX-portable.
3. **Validate.** Run `scripts/kak-validate.sh` against the script. Fix anything it reports and re-run until clean.
4. **Report.** Show the final script and what the validation run confirmed. If `kak` is unavailable, say so explicitly rather than claiming success.

## Validation

`scripts/kak-validate.sh` sources a script in `kak -ui json` (headless — no display, no user rc) and reports problems. It applies two structural gates before the generic load/run check:

- **Kakoune-DSL gate.** The file must contain at least one Kakoune keyword (`define-command`, `define-key`, `hook`, `add-highlighter`, `declare-option`, `execute-keys`, `evaluate-commands`, `set-option`, `%sh{`, `%val{`). Pure shell or prose fails here, so a model that answers in the wrong language is caught rather than passing by accident.
- **`--require TOKEN` gate.** The caller supplies the constructs the task demands; the script must contain each one or it fails. This is the caller's contract, not baked-in truth — e.g. `--require define-command` rejects a script that used `define-key` instead.

```bash
# Source the script, then exit with a non-zero code on any error.
scripts/kak-validate.sh path/to/my.kak

# Source it AND run a command in the same headless session to exercise it.
scripts/kak-validate.sh path/to/my.kak "my-command arg"

# Require the script to contain specific constructs before it can pass.
scripts/kak-validate.sh --require define-command --require execute-keys path/to/my.kak
```

| Exit code | Meaning |
| --- | --- |
| `0` | Script loaded and (if a command was given) ran cleanly. |
| `1` | Load/parse/runtime error, `kak` hang, wrong construct, a required `--require` token missing, or output not recognisable Kakoune code. |
| `2` | Usage error (bad flags) or `kak` not on `PATH`. |

## Evals

`evals/evals.json` holds the task fixtures used to measure this skill end to end: each entry has an `instruction`, a `ground_truth`, and a `note` explaining the trip-up under test. The fixtures cover both trip-ups — `execute-keys` vs `evaluate-commands`, and command parsing — plus options, highlighters, hooks, completers, and a broken-script fix.

Render a benchmark run into a standalone HTML report:

```bash
python3 evals/viewer.py results.json evals.json -o report.html
```

`results.json` is a JSON array of `[idx, kind, result, detail, block]` tuples; `viewer.py` attaches each task's instruction and ground truth from `evals.json`. Run the skill against the fixtures, then point the viewer at the results.

## Conventions

- **Naming.** Prefix every option and command with the script name (or a one-word description of its purpose). Options use `_` as a separator; commands use `-`. Option names allow only `[a-zA-Z0-9_]`.
- **`-docstring`** every non-hidden command and option, so completion shows its purpose.
- **POSIX shell** inside `%sh{}`: `printf` over `echo`, `${var##*/}` over `basename`, `[` over `[[`, `>/dev/null 2>&1` over `&>`, `expr` over `=~`.
- **Background processes** must detach cleanly: `{ command } </dev/null >/dev/null 2>&1 &`.
- **Guard boundaries.** Validate user input and external output — shell is a supported expansion target.
- Follow the structure of the bundled `examples/*.kak` scripts; they are the reference for real-world layout.

## Project structure

| Path | Purpose |
| --- | --- |
| `SKILL.md` | Agent-facing manifest and authoring spec (frontmatter + workflow). |
| `agents/kakoune-expert.md` | Expert subagent that owns the author → validate workflow end to end. |
| `scripts/kak-validate.sh` | Headless validator; the final gate before handover. |
| `references/` | Condensed docs for each Kakoune subsystem — read before touching it. |
| `examples/*.kak` | Real-world scripts (`git`, `clang`, `ctags`, `fifo`, `spell`, `c-family`). |
| `evals/evals.json` | Task fixtures with ground truth. |
| `evals/viewer.py` | Renders a benchmark run (`results.json`) to an HTML report. |

The `references/` map to the two trip-ups and the rest of the command model: `execeval.md` (execute-keys vs evaluate-commands, `local` scope), `command-parsing.md` (quoting, balanced strings, typed expansions), `scripting.md` (POSIX shell, FIFOs, async, completers), plus `mapping`, `options`, `hooks`, `highlighters`, `registers`, `faces`, and `expansions`.

## Contributing

- **Add a script.** Mirror the structure of an existing `examples/*.kak`: prefix names, add `-docstrings`, keep `%sh{}` POSIX-portable, then validate with `scripts/kak-validate.sh`.
- **Add an eval.** Append one object to `evals/evals.json` with `instruction`, `ground_truth`, and a `note` naming the trip-up. Keep it to one construct so the failure mode is unambiguous.
- **Add a reference.** Condense one upstream Kakoune doc page into `references/` — instructions over descriptions, grounded in the upstream link, no memory-based claims.

## Links

- Kakoune source + docs: `https://github.com/mawww/kakoune`
- Command parsing: `https://github.com/mawww/kakoune/blob/master/doc/pages/command-parsing.asciidoc`
- Writing scripts: `https://github.com/mawww/kakoune/blob/master/doc/writing_scripts.asciidoc`
- Interfacing / FIFOs / async: `https://github.com/mawww/kakoune/blob/master/doc/interfacing.asciidoc`

## Verify

Prove the skill's structure is intact and its constraints are present:

```bash
# Manifest, executable validator, reference coverage, example scripts all present.
test -f SKILL.md && test -f agents/kakoune-expert.md \
  && test -x scripts/kak-validate.sh \
  && [ "$(ls references/*.md | wc -l)" -ge 8 ] \
  && [ "$(ls examples/*.kak | wc -l)" -ge 4 ] \
  && grep -q 'execute-keys' SKILL.md && grep -q 'evaluate-commands' SKILL.md

# README itself: at least 7 top-level sections.
grep -c '^## ' README.md
```
