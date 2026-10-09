#!/usr/bin/env bash
# Source adapted:
#  - [tmux-real-user-testing](https://github.com/QwenLM/qwen-code/blob/main/.qwen/skills/tmux-real-user-testing/SKILL.md) - Run Qwen Code in a real tmux TUI session as a user would.
set -euo pipefail

usage() {
  cat <<'EOF'
tmux-real-user-log.sh - helper for readable tmux real-user test logs (Kakoune)

Usage:
  tmux-real-user-log.sh start <scenario> <workdir> <command...>
  tmux-real-user-log.sh snapshot <session> <outdir> <label> [scrollback]
  tmux-real-user-log.sh send <session> <keys...>
  tmux-real-user-log.sh type-submit <session> <text>
  tmux-real-user-log.sh kak-cmd <session> <command>
  tmux-real-user-log.sh wait-for <session> <outdir> <regex> [attempts] [sleep_seconds] [scrollback]
  tmux-real-user-log.sh finish <session> <outdir>
  tmux-real-user-log.sh help

# TODO(FIXME): the per-char delay below (0.03s) and the <=8 char threshold for
# the fast single send-keys path are hard empirics, not derived values. The
# whole scenario hinges on Kakoune's command-line NOT dropping bulk input, so
# these two numbers are the first place to retune if a long `set-option`
# command silently truncates mid-typing. If a command appears to land
# partially, lower the threshold (e.g. 16) or raise per-char delay (e.g. 0.05);
# verify by snapshotting the command-line right after kak-cmd returns.

Examples:
  eval "$(tmux-real-user-log.sh start mytest . ./src/kak test.txt)"
  tmux-real-user-log.sh snapshot mytest-... /tmp/run "01 initial screen"
  tmux-real-user-log.sh type-submit mytest-... /auth
  tmux-real-user-log.sh send mytest-... Down Enter
  tmux-real-user-log.sh kak-cmd mytest-... "set-option autoindent"
  tmux-real-user-log.sh wait-for mytest-... /tmp/run "-- Normal --"
  tmux-real-user-log.sh finish mytest-... /tmp/run
EOF
}

require_args() {
  local need=$1
  local got=$2
  local cmd=$3
  if (( got < need )); then
    echo "error: '$cmd' expects at least $need args, got $got" >&2
    usage >&2
    exit 2
  fi
}

cmd=${1:-help}
shift || true

case "$cmd" in
  start)
    require_args 3 $# start
    scenario=$1
    workdir=$2
    shift 2
    ts=$(date +%Y%m%d-%H%M%S)
    outdir="$workdir/tmp/${scenario}-tmux-${ts}"
    session="${scenario}-tmux-${ts}"

    # Fail early if session name already exists
    if tmux has-session -t "$session" 2>/dev/null; then
      echo "error: tmux session '$session' already exists" >&2
      exit 1
    fi

    mkdir -p "$outdir"
    # Join args with plain spaces (NOT printf '%q'): tmux passes the command to
    # /bin/sh -c, and shell-escaping would collapse a path containing spaces
    # (e.g. "./src/kak -n") into a single token that sh cannot find.
    shell_command=$(printf '%s ' "$@")
    tmux new-session -d -s "$session" -x 200 -y 50 -c "$workdir" "$shell_command"
    # Eval-friendly: source this output to get SESSION/OUTDIR/LOG in your shell
    printf 'export SESSION=%q\n' "$session"
    printf 'export OUTDIR=%q\n' "$outdir"
    printf 'export LOG=%q/tmux-readable-full.log\n' "$outdir"
    ;;

  snapshot)
    require_args 3 $# snapshot
    session=$1
    outdir=$2
    label=$3
    scrollback=${4:--300}
    mkdir -p "$outdir"
    log="$outdir/tmux-readable-full.log"
    {
      printf '\n===== %s =====\n' "$label"
      tmux capture-pane -t "$session" -p -S "$scrollback"
    } >> "$log"
    ;;

  send)
    require_args 2 $# send
    session=$1
    shift
    for key in "$@"; do
      tmux send-keys -t "$session" -- "$key"
      sleep 0.15
    done
    ;;

  type-submit)
    require_args 2 $# type-submit
    session=$1
    text=$2
    tmux send-keys -t "$session" -- "$text"
    sleep 0.5
    tmux send-keys -t "$session" Enter
    ;;

  kak-cmd)
    # Send a Kakoune command-line command: ':' + command text + Enter.
    # Splits the ':' and Enter so Kakoune's command-line does not drop input.
    #
    # IMPORTANT: Kakoune's command-line drops rapid *bulk* input, so long
    # commands (e.g. `set-option global llm_args --tokens Hello world
    # --delay 0.05`) are typed one character at a time with a small per-char
    # delay. Single-token / short commands keep the fast single send-keys path.
    require_args 2 $# kak-cmd
    session=$1
    command=$2
    per_char_delay=${TMUX_PER_CHAR_DELAY:-0.03}
    tmux send-keys -t "$session" -- ':'
    sleep 0.15
    if [ "${#command}" -le 8 ]; then
      # Short command: single delivery is reliable enough.
      tmux send-keys -t "$session" -- "$command"
      sleep 0.5
    else
      # Long command: type each character individually to avoid dropped input.
      for (( i=0; i<${#command}; i++ )); do
        tmux send-keys -t "$session" -- "${command:$i:1}"
        sleep "$per_char_delay"
      done
      sleep 0.3
    fi
    tmux send-keys -t "$session" Enter
    ;;

  wait-for)
    require_args 3 $# wait-for
    session=$1
    outdir=$2
    regex=$3
    attempts=${4:-60}
    sleep_seconds=${5:-2}
    scrollback=${6:--400}
    echo "[WAIT-FOR DBG] session=$session outdir=$outdir regex=$regex attempts=$attempts sleep=$sleep_seconds scrollback=$scrollback" >&2
    mkdir -p "$outdir"
    for i in $(seq 1 "$attempts"); do
      echo "[WAIT-FOR DBG] attempt $i/$attempts" >&2
      sleep "$sleep_seconds"
      tmux capture-pane -t "$session" -p -S "$scrollback" > "$outdir/current-pane.txt"
      if grep -Eq -- "$regex" "$outdir/current-pane.txt"; then
        echo "[WAIT-FOR DBG] MATCH on attempt $i" >&2
        cat "$outdir/current-pane.txt"
        exit 0
      fi
    done
    echo "[WAIT-FOR DBG] NO MATCH after $attempts attempts" >&2
    cat "$outdir/current-pane.txt"
    exit 1
    ;;

  finish)
    require_args 2 $# finish
    session=$1
    outdir=$2
    log="$outdir/tmux-readable-full.log"
    tmux capture-pane -t "$session" -p -S -10000 > "$outdir/tmux-final-capture.log"
    {
      printf '\n===== final capture before cleanup =====\n'
      cat "$outdir/tmux-final-capture.log"
    } >> "$log"
    tmux kill-session -t "$session"
    wc -l "$log" "$outdir/tmux-final-capture.log"
    ;;

  help|-h|--help)
    usage
    ;;

  *)
    echo "error: unknown command '$cmd'" >&2
    usage >&2
    exit 2
    ;;
esac
