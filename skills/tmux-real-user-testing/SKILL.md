---
name: tmux-real-user-testing
description: |
  This skill should be used when the user asks to "do real testing with tmux", "save tmux logs", "test Kakoune like a real user", "generate a reviewable TUI test report", "test Kakoune command interaction", or requests a tmux-based real-user E2E run with complete readable logs. It guides real TUI usage with step-by-step capture-pane snapshots rather than ANSI raw pipe logs.
---

# tmux Real User Testing

Run the Kakoune editor in a real tmux TUI session as a user would: edit files,
navigate between modes, exercise `:` commands, drive keymaps and hooks, and save
a readable log that maintainers can review. Prefer this workflow when the goal
is not just a pass/fail assertion, but a narrative artifact showing what
happened on screen.

## Core principle

Use tmux as a real-use harness. Drive Kakoune with realistic keystrokes, then
save a step-by-step readable transcript with `tmux capture-pane -p` after each
meaningful state change.

Avoid relying on `tmux pipe-pane` as the primary report. `pipe-pane` captures
raw ANSI/control streams from Kakoune's terminal UI and often looks like
garbled text when opened as plain text. Use `pipe-pane` only as an optional
forensic artifact. Make `tmux-readable-full.log` the main deliverable.

## When to use

Use this workflow for:

- Kakoune TUI behavior: mode transitions, selection manipulation, command-line
  input, and buffer editing flows.
- Realistic editing workflows where a maintainer wants to read the journey
  afterward (e.g., multi-cursor edits, undo/redo, register operations).
- Regression testing where final state is insufficient and intermediate screens
  matter (e.g., a command that briefly flashes a status message).
- User-facing flows such as `:llm-*` commands, filetype detection, hooks,
  options, windowing (`tmux-terminal-*`, `tmux-repl-*`), and interactive error
  recovery.

Use headless JSON E2E (`test/e2e/`) instead when only command execution or
`-ui json` protocol behavior needs structured assertions.

## Standard artifact layout

Create a timestamped directory under project `tmp/`:

```text
tmp/<scenario>-tmux-YYYYMMDD-HHMMSS/
├── tmux-readable-full.log   # primary report: step-by-step readable snapshots
├── tmux-final-capture.log   # final screen only
├── current-pane.txt         # latest poll/snapshot scratch file
└── report.md                # short summary with result and artifact pointers
```

Do not overwrite previous runs. Preserve complete logs unless the user explicitly
asks to sanitize or trim them.

## Recommended helper script

Use `scripts/tmux-real-user-log.sh` to avoid rewriting shell glue. The script can
start a session, append labeled snapshots, send keys, wait for text, and finish.

The `start` command outputs `export` statements — use `eval` to set the variables
directly in your shell:

```bash
eval "$(bash ~/.agents/skills/tmux-real-user-testing/scripts/tmux-real-user-log.sh \
  start <scenario> . ./src/kak <file>)"
# → $SESSION, $OUTDIR, $LOG are now available
```

Show the full usage before running a new scenario:

```bash
bash ~/.agents/skills/tmux-real-user-testing/scripts/tmux-real-user-log.sh help
```

## Manual workflow

### 1. Start Kakoune in a tmux session

Use a large tmux viewport so Kakoune's status line and message area render
fully. Wait for Kakoune to be ready — poll for the Kakoune prompt (`-- Normal --`
or the status line) rather than blind sleeping:

```bash
TS=$(date +%Y%m%d-%H%M%S)
PROJECT_ROOT="$(pwd)"
OUT="$PROJECT_ROOT/tmp/<scenario>-$TS"
SESSION="<scenario>-$TS"
mkdir -p "$OUT"
tmux new-session -d -s "$SESSION" -x 200 -y 50 \
  -c "$PROJECT_ROOT" \
  "./src/kak <file>"

# Poll until Kakoune is ready (Kakoune's normal-mode status line)
for i in $(seq 1 30); do
  sleep 1
  if tmux capture-pane -t "$SESSION" -p -S -100 | grep -q -- "-- Normal --"; then
    break
  fi
done
```

Use `kak -ui json` only when verifying headless JSON-RPC behavior. Use the
globally installed `kak` only when reproducing a user-reported installed-version
bug.

### 2. Append labeled readable snapshots

After each meaningful action, append a section header plus `capture-pane -p` to
the full log:

```bash
LOG="$OUT/tmux-readable-full.log"
{
  printf '\n===== 01 open file, normal mode =====\n'
  tmux capture-pane -t "$SESSION" -p -S -240
} >> "$LOG"
```

Increase `-S` as the session grows (add ~100 lines per section). The important
part is that each section is a rendered frame, not raw ANSI output.

### 3. Send keys like a user

Split typing and Enter to avoid swallowed submissions:

```bash
# Enter command-line mode and type a command
tmux send-keys -t "$SESSION" ':'
tmux send-keys -t "$SESSION" 'set-option'
tmux send-keys -t "$SESSION" ' '
tmux send-keys -t "$SESSION" 'autoindent'
sleep 0.5
tmux send-keys -t "$SESSION" Enter
sleep 1
```

For navigation and mode transitions:

```bash
# Move down, enter insert mode, type text
tmux send-keys -t "$SESSION" j
tmux send-keys -t "$SESSION" i
sleep 0.3
tmux send-keys -t "$SESSION" 'hello kakoune'
tmux send-keys -t "$SESSION" Escape
```

For text input into the command-line, prefer one key at a time if bulk text is
ignored (Kakoune's command-line sometimes drops rapid input):

```bash
tmux send-keys -t "$SESSION" a
tmux send-keys -t "$SESSION" u
tmux send-keys -t "$SESSION" x
```

### 4. Poll for completion instead of blind sleeping

Use text on the screen as the completion condition. On timeout, dump the current
pane so the log shows what was on screen when the wait expired:

```bash
for i in $(seq 1 60); do
  sleep 2
  tmux capture-pane -t "$SESSION" -p -S -400 > "$OUT/current-pane.txt"
  if grep -q "error\|Error\|fail" \
    "$OUT/current-pane.txt"; then
    break
  fi
done
# Always append the final poll result (match or timeout) to the log
{
  printf '\n===== 04 command result =====\n'
  cat "$OUT/current-pane.txt"
} >> "$LOG"
```

### 5. Finish cleanly

Capture the final screen, append it, then kill the session:

```bash
tmux capture-pane -t "$SESSION" -p -S -10000 > "$OUT/tmux-final-capture.log"
{
  printf '\n===== final capture before cleanup =====\n'
  cat "$OUT/tmux-final-capture.log"
} >> "$LOG"
tmux kill-session -t "$SESSION"
```

## Reporting expectations

Write `report.md` with:

- Date, tmux session name, Kakoune binary, workspace, and target file.
- Scenario scope and exact steps tested.
- PASS/FAIL result.
- Key screen observations and important state transitions (mode changes,
  selection counts, status-line messages).
- Artifact list, with `tmux-readable-full.log` marked as the primary log.
- Any known side effects, such as file writes, settings updates, or spawned
  tmux panes/windows.

Keep assertions tied to evidence in the log. Prefer phrases like "log section
`03 set autoindent` shows `-- Normal --` with no error" over unsupported
summaries.

## Designing a test scenario

A good scenario is a linear sequence of observable state transitions. Design it
as a series of steps where each step produces visible Kakoune output you can
capture:

1. **Entry point** — the file, command, or key sequence that starts the flow.
2. **Mode transitions** — normal → insert → select → command-line → multi.
3. **Waiting states** — slow file loads, hook execution, or external commands
   that need `wait-for` polling.
4. **Confirmation** — success/error text or a mode indicator visible on screen
   that marks completion.
5. **Side effects** — external actions the flow triggers (file writes, config
   changes, new tmux panes) that may affect subsequent runs.

For each step, define:

- The **keys** to send (`:`, `i`, `j`, `Escape`, `:set-option ...<ret>`, etc.)
- The **expected text** to wait for (`-- Normal --`, `error`, `Saved`)
- When to take a **snapshot** (before and after every interaction)

### Short example: testing `:set-option` → verify mode

```bash
HELPER=~/.agents/skills/tmux-real-user-testing/scripts/tmux-real-user-log.sh

# Start
eval "$(bash "$HELPER" start setopt-test . ./src/kak test.txt)"
# → prints SESSION=... OUTDIR=...

# Send a Kakoune command-line command (:set-option autoindent)
bash "$HELPER" kak-cmd "$SESSION" "set-option autoindent"
bash "$HELPER" snapshot "$SESSION" "$OUTDIR" "01 set-option command"

# Verify Kakoune returned to Normal mode
bash "$HELPER" wait-for "$SESSION" "$OUTDIR" -- Normal --
bash "$HELPER" snapshot "$SESSION" "$OUTDIR" "02 back to normal"

# Finish
bash "$HELPER" finish "$SESSION" "$OUTDIR"
```

For flows involving external commands or tmux pane splits (e.g. `:llm-*`,
`:tmux-terminal-*`), the `wait-for` poll will catch the result after the
external step completes. If the flow requires spawning a new tmux window or
pane, note that side effect in the scenario design.

## Safety and privacy

Ask before deleting logs or reverting settings. Do not sanitize by default if
the user explicitly requests complete logs. If logs may be shared externally,
offer a separate sanitized copy rather than modifying the original.

Mention likely side effects before starting: editing a file will modify it on
disk; `:tmux-terminal-*` and `:tmux-repl-*` commands will spawn new tmux panes
or windows; hooks may run external commands.

## Common pitfalls

- `capture-pane` records current rendered state, not transient flicker.
- Kakoune's command-line sometimes drops rapid bulk input; send characters
  individually for `:` commands.
- `tmux-final-capture.log` contains only the last screen; it is not the full
  journey.
- `tmux-readable-full.log` is the report-grade artifact.
- `tmux pipe-pane` raw logs can contain ANSI control sequences and look garbled.
- Use a timestamped output directory for every run to avoid overwriting evidence.
- Kakoune's `-- Normal --` / `-- Insert --` / `-- Select --` status line is the
  reliable readiness/transition signal, not arbitrary text.
