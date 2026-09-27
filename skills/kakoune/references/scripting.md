# Scripting — conventions, POSIX shell, FIFOs, async, completers

Condensed reference for building scripts that interface with external programs.
Sources: `https://github.com/mawww/kakoune/tree/master/doc/writing_scripts.asciidoc`,
`https://github.com/mawww/kakoune/tree/master/doc/interfacing.asciidoc`, `https://github.com/mawww/kakoune/tree/master/doc/pages/buffers.asciidoc`.

## Design principle

A script is an **API to external software**, not a tool itself. Keep
implementations minimal and dependencies small and expected (e.g. `clang.kak`
→ `clang`, `tmux.kak` → `tmux`).

## Naming

- Prefix every option and command with the script name (or a one-word
  description of its purpose).
- **Options** use `_` as a separator (so shell scopes can use them).
- **Commands** use `-` as a separator.
- Non-hidden commands and options must carry a `-docstring`.

## POSIX shell (inside `%sh{}`)

| Avoid (bashism) | Use (POSIX) |
| --- | --- |
| `echo "..."` with ambiguous content | `printf %s\n "..."` / `printf "value: %s\n" "..."` |
| `$(basename "$var")` | `${var##*/}` |
| `[[ ... ]]` | `[ ... ]` |
| `&>` | `>/dev/null 2>&1` |
| `[[ $x =~ re ]]` | `expr "$x" : 'pattern'` (matches whole string; `^`/`$` anchors are undefined) |

## Background processes

Detach cleanly so the process survives the shell scope that spawned it:

```sh
{
    command
} </dev/null >/dev/null 2>&1 &
```

If a `%sh{}` runs a long-running process, Kakoune waits for stdout/stderr to be
closed — redirect them so the expansion doesn't block.

## Async socket writes

The Kakoune Unix stream socket is at
`/tmp/kakoune/${username}/${kak_session}` (from `$kak_session`). Write commands
to it via `kak -p`:

```kakoune
nop %sh{ {
    sleep 10
    echo "eval -client '$kak_client' 'echo sleep ended'" |
        kak -p ${kak_session}
} > /dev/null 2>&1 < /dev/null & }
```

- `nop` prevents any `%sh{}` output from being interpreted as commands.
- Writing to the socket has no user-interface context, so use `eval -client`
  to target a specific client.
- The brace subshell + `/dev/null` redirects + trailing `&` make it run
  asynchronously.

## FIFO buffer pattern (display async output in a buffer)

```kakoune
evaluate-commands %sh{
    # Create a temporary fifo for communication
    output=$(mktemp -d -t kak-temp-XXXXXXXX)/fifo
    mkfifo ${output}
    # run command detached from the shell
    { run command here > ${output} } > /dev/null 2>&1 < /dev/null &
    # Open the file in Kakoune and add a hook to remove the fifo
    echo "edit! -fifo ${output} *buffer-name*
          hook buffer BufClose .* %{ nop %sh{ rm -r $(dirname ${output})} }"
}
```

- `edit! -fifo <filename> <buffername>` creates a buffer that reads from the
  FIFO and updates as it's written. `-scroll` keeps the newest data visible.
- On FIFO close the buffer becomes a scratch buffer; on buffer delete Kakoune
  closes the read end (writers get `SIGPIPE`).
- Add `set buffer filetype <...>` to the echoed command so filetype hooks apply.
- Clean up with a `BufClose` hook (`-always` if you want it regardless of
  drafts). See `buffers.asciidoc` and `hooks.asciidoc`.

## Completion candidates (external completers)

Candidate format:

```plaintext
line.column[+len]@timestamp candidate1|select1|menu1 candidate2|select2|menu2 ...
```

- First element: where/when the completion applies.
- Each candidate is a triplet `<text>|<select cmd>|<menu text>`.
- `<select cmd>` runs when the item is selected (often `info -placement menu ...`).
- `<menu text>` is a markup string (may contain `{face}` directives).

Wire it up:

```kakoune
# store the candidate list
decl completions plugin_completions
# register it for a filetype
hook global BufSetOption filetype=my_filetype %{
    set -add buffer completers option=plugin_completions
}
```

Compute asynchronously, then write back to the triggering buffer's socket:

```sh
completions="$line.$column@$kak_timestamp $candidates"
echo "set buffer=${kak_bufname} plugin_completions $completions" |
    kak -p ${kak_session}
```

## Scratch & debug buffers

- **Scratch** (`:edit -scratch`): not file-linked, no unsaved-changes warning,
  `:write` needs an explicit filename. `\*scratch*` is created at startup with
  no buffers open.
- **Debug** (`:edit -debug`): gathers diagnostics; skipped in buffer cycling,
  excluded from `word=all` completion, some hooks don't fire, display profiling
  off. Errors/warnings and `:debug` / `:echo-debug` output land in `\*debug*`.
