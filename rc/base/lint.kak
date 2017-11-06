declare-option -docstring %{shell command to which the path of a copy of the current buffer will be passed
The output returned by this command is expected to comply with the following format:
 {filename}:{line}:{column}: {kind}: {message}} \
    str lintcmd

declare-option -hidden line-specs lint_flags
declare-option -hidden range-specs lint_errors

define-command lint -docstring 'Parse the current buffer with a linter' %{
    %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-lint.XXXXXXXX)
        mkfifo "$dir"/fifo
        printf '%s\n' "evaluate-commands -no-hooks write $dir/buf"

        printf '%s\n' "evaluate-commands -draft %{
                  edit! -fifo $dir/fifo -debug *lint-output*
                  set-option buffer filetype make
                  set-option buffer make_current_error_line 0
                  hook -group fifo buffer BufCloseFifo .* %{
                      nop %sh{ rm -r '$dir' }
                      remove-hooks buffer fifo
                  }
              }"

        { # do the parsing in the background and when ready send to the session

        eval "$kak_opt_lintcmd '$dir'/buf" | sort -t: -k2,2 -n > "$dir"/stderr
        printf '%s\n' "evaluate-commands -client $kak_client echo 'linting done'" | kak -p "$kak_session"

        # Flags for the gutter:
        #   line3|{red}:line11|{yellow}
        # Contextual error messages:
        #   l1,c1,err1
        #   ln,cn,err2
        awk -F: -v file="$kak_buffile" -v stamp="$kak_timestamp" '
            /:[0-9]+:[0-9]+: ([Ff]atal )?[Ee]rror/ {
                flags = flags $2 "|{red}█:"
            }
            /:[0-9]+:[0-9]+:/ {
                if ($4 !~ /[Ee]rror/) {
                    flags = flags $2 "|{yellow}█:"
                }
            }
            /:[0-9]+:[0-9]+:/ {
                errors = errors ":" $2 "." $3 "," $2 "." $3 "|" substr($4,2)
                # fix case where $5 is not the last field because of extra :s in the message
                for (i=5; i<=NF; i++) errors = errors "\\:" $i
                errors = substr(errors, 1, length(errors)-1) " (col " $3 ")"
            }
            END {
                print "set-option \"buffer=" file "\" lint_flags  %{" stamp ":" substr(flags,  1, length(flags)-1)  "}"
                errors = substr(errors, 1, length(errors)-1)
                gsub("~", "\\~", errors)
                print "set-option \"buffer=" file "\" lint_errors %~" stamp errors "~"
            }
        ' "$dir"/stderr | kak -p "$kak_session"

        cut -d: -f2- "$dir"/stderr | sed "s@^@$kak_bufname:@" > "$dir"/fifo

        } >/dev/null 2>&1 </dev/null &
    }
}

define-command -hidden lint-show %{
    update-option buffer lint_errors
    %sh{
        desc=$(printf '%s\n' "$kak_opt_lint_errors" | sed -e 's/\([^\\]\):/\1\n/g' | tail -n +2 |
               sed -ne "/^$kak_cursor_line\.[^|]\+|.*/ { s/^[^|]\+|//g; s/'/\\\\'/g; s/\\\\:/:/g; p; }")
        if [ -n "$desc" ]; then
            printf '%s\n' "info -anchor $kak_cursor_line.$kak_cursor_column '$desc'"
        fi
    } }

define-command lint-enable -docstring "Activate automatic diagnostics of the code" %{
    add-highlighter window flag_lines default lint_flags
    hook window -group lint-diagnostics NormalIdle .* %{ lint-show }
}

define-command lint-disable -docstring "Disable automatic diagnostics of the code" %{
    remove-highlighter window/hlflags_lint_flags
    remove-hooks window lint-diagnostics
}

define-command lint-next-error -docstring "Jump to the next line that contains an error" %{
    update-option buffer lint_errors
    %sh{
        printf '%s\n' "$kak_opt_lint_errors" | sed -e 's/\([^\\]\):/\1\n/g' | tail -n +2 | {
            while IFS='|' read -r candidate rest
            do
                first_range=${first_range-$candidate}
                if [ "${candidate%%.*}" -gt "$kak_cursor_line" ]; then
                    range=$candidate
                    break
                fi
            done
            range=${range-$first_range}
            if [ -n "$range" ]; then
                printf '%s\n' "select $range"
            else
                printf 'echo -markup "{Error}no lint diagnostics"\n'
            fi
        }
    }}

define-command lint-previous-error -docstring "Jump to the previous line that contains an error" %{
    update-option buffer lint_errors
    %sh{
        printf '%s\n' "$kak_opt_lint_errors" | sed -e 's/\([^\\]\):/\1\n/g' | tail -n +2 | sort -t. -k1,1 -rn | {
            while IFS='|' read -r candidate rest
            do
                first_range=${first_range-$candidate}
                if [ "${candidate%%.*}" -lt "$kak_cursor_line" ]; then
                    range=$candidate
                    break
                fi
            done
            range=${range-$first_range}
            if [ -n "$range" ]; then
                printf '%s\n' "select $range"
            else
                printf 'echo -markup "{Error}no lint diagnostics"\n'
            fi
        }
    }}
