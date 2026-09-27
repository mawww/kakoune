#!/bin/sh
# kak-validate.sh — headlessly validate a Kakoune script (and optional command).
#
# Usage:
#   kak-validate.sh SCRIPT            # source SCRIPT; non-zero exit on any error
#   kak-validate.sh SCRIPT "COMMAND"  # source SCRIPT, then run COMMAND; same
#   kak-validate.sh --require TOKEN [TOKEN ...] SCRIPT [COMMAND]
#                                     # also require each TOKEN to appear in SCRIPT
#
# Exit codes:
#   0  SCRIPT loaded and ran (and COMMAND ran) without error
#   1  load error, parse/syntax error, runtime error, hang, wrong construct,
#      missing required token, or not recognisable Kakoune code
#   2  usage / environment error (kak not on PATH, script file missing)
#
# Two structural gates precede the generic load/run check (the final gate):
#
#   1. Kakoune-DSL gate: the file must contain at least one Kakoune statement
#      keyword (define-command, define-key, hook, add-highlighter,
#      declare-option, execute-keys, evaluate-commands, set-option, %sh{, %val{).
#      A pure shell script or prose therefore fails here — a model that emits
#      non-Kakoune output is rejected even when that output loads and runs
#      cleanly under a plain shell.
#
#   2. Required-token gate (--require): every supplied TOKEN must appear in the
#      file. This catches wrong command names (e.g. leaving the original broken
#      name instead of the fixed one) and wrong constructs (e.g. define-key
#      instead of define-command). The validator stays generic: it does not know
#      ground truth, so the expected tokens are supplied as arguments.
#
# How it works: Kakoune is launched headlessly with `-ui json` and the `-n` flag
# (no user rc). A RuntimeError hook plus a `try { source } catch` around the
# script capture the failure into a file, and `kill! 1` forces a non-zero exit.
# A `timeout` guards against scripts that hang (a hook alone can hang Kakoune on
# parse errors, so the try/catch is mandatory).

set -u

KAK_BIN="$(command -v kak || true)"
if [ -z "$KAK_BIN" ]; then
  echo "kak-validate: 'kak' not found on PATH" >&2
  exit 2
fi

# Collect required tokens (repeatable --require / --require=) that must appear
# in SCRIPT, before the positional SCRIPT/COMMAND. Tokens are newline-separated
# so each can contain any character except a newline.
require_tokens=""
while [ $# -gt 0 ]; do
  case "$1" in
    --require)
      shift
      if [ $# -eq 0 ]; then
        echo "usage: $0 [--require TOKEN ...] SCRIPT [COMMAND]" >&2
        exit 2
      fi
      require_tokens="$require_tokens
$1"
      ;;
    --require=*)
      require_tokens="$require_tokens
${1#--require=}"
      ;;
    -h|--help)
      sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "kak-validate: unknown option: $1" >&2
      exit 2
      ;;
    *)
      break
      ;;
  esac
  shift
done

SCRIPT="${1:-}"
CMD="${2:-}"

if [ -z "$SCRIPT" ]; then
  echo "usage: $0 [--require TOKEN ...] SCRIPT [COMMAND]" >&2
  exit 2
fi
if [ ! -f "$SCRIPT" ]; then
  echo "kak-validate: script not found: $SCRIPT" >&2
  exit 2
fi
# Resolve to an absolute path before cd-ing into the temp workspace, so a
# relative SCRIPT argument is found regardless of the working directory.
case "$SCRIPT" in
  /*) abs_script="$SCRIPT" ;;
  *)  abs_script="$PWD/$SCRIPT" ;;
esac

# Read the file once; both structural gates and the load/run check reuse it.
dsl_content="$(cat "$abs_script" 2>/dev/null)"

# --- Structural gate 1: Kakoune-DSL gate ---
# Reject output that is not recognisable Kakoune code (a pure shell script,
# prose, etc.). The generic load/run check below would otherwise accept a
# shell script that merely runs cleanly under a plain shell.
case "$dsl_content" in
  *define-command*|*define-key*|*"hook "*|*add-highlighter*|*declare-option*|\
   *execute-keys*|*evaluate-commands*|*set-option*|*'%sh{'*|*'%val{'*)
    ;;
  *)
    echo "kak-validate: not recognisable Kakoune code (no define-command/hook/add-highlighter/declare-option/execute-keys/evaluate-commands/set-option/%sh/%val)" >&2
    exit 1
    ;;
esac

# --- Structural gate 2: required-token gate (--require) ---
# Every supplied token must appear in the file. This catches a wrong command
# name (e.g. leaving the original broken name instead of the fixed one) or a
# wrong construct (e.g. define-key instead of define-command). The validator is
# generic; the expected tokens are supplied by the caller via --require.
if [ -n "$require_tokens" ]; then
  while IFS= read -r _req; do
    [ -z "$_req" ] && continue
    case "$dsl_content" in
      *"$_req"*) ;;
      *)
        echo "kak-validate: required token not found: $_req" >&2
        exit 1
        ;;
    esac
  done <<EOF
$require_tokens
EOF
fi

TIMEOUT_SECS="${KAK_VALIDATE_TIMEOUT:-10}"
# Dot-free mktemp template on purpose: a '.' is not a valid Kak session-name
# character, so the work-dir basename used below for session-id entropy must
# contain only [A-Za-z0-9_-].
work="$(mktemp -d "${XDG_RUNTIME_DIR:-/tmp}/kakvalXXXXXXXX")"
# Session id combines the PID and the mktemp work-dir name: unique per run.
session="kaval-$$_${work##*/}"
err_raw=""

cleanup() {
  rm -f "$work/src.kak" "$work/err" "$work/out.txt" "$work/err.txt" 2>/dev/null || true
  rm -rf "$work" 2>/dev/null || true
  rm -f "${XDG_RUNTIME_DIR:-/tmp}/kakoune/$session" 2>/dev/null || true
}
trap cleanup EXIT

# Build the -e sequence. The leading "N:M: 'source': " position prefix that
# Kakoune adds when reporting through `source` is stripped before display.
seq="hook global RuntimeError \"\\d+:\\d+: (.+)\" %{ echo -to-file err -end-of-line %val{error}; kill! 1 }; try %{ source src.kak } catch %{ echo -to-file err -end-of-line %val{error}; kill! 1 }"
if [ -n "$CMD" ]; then
  seq="$seq; try %{ $CMD } catch %{ echo -to-file err -end-of-line %val{error}; kill! 1 }"
fi
seq="$seq; quit!"

cd "$work" || exit 2
cp "$abs_script" "$work/src.kak"

timeout "$TIMEOUT_SECS" "$KAK_BIN" -s "$session" -ui json -n -e "$seq" >"$work/out.txt" 2>"$work/err.txt"
rc=$?

if [ -f "$work/err" ]; then
  err_raw="$(cat "$work/err" 2>/dev/null)"
fi
cleanup

if [ "$rc" -eq 124 ]; then
  echo "kak-validate: timed out after ${TIMEOUT_SECS}s (script hung)" >&2
  exit 1
fi

if [ "$rc" -ne 0 ]; then
  clean="$(printf '%s' "$err_raw" | sed -e 's/^[0-9]*:[0-9]*: //' -e "s/^'[^']*': //")"
  echo "kak-validate: failed (exit $rc)" >&2
  [ -n "$clean" ] && echo "  $clean" >&2
  exit 1
fi

exit 0
