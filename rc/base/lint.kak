# your linter should output in this format:
# {filename}:{line}:{column}: {kind}: {message}
decl str lintcmd

decl -hidden line-flags lint_flags
decl -hidden str        lint_errors

def lint -docstring 'Parse the current buffer with a linter' %{
    %sh{
        dir=$(mktemp -d -t kak-lint.XXXXXXXX)
        mkfifo "$dir"/fifo
        printf '%s\n' "eval -no-hooks write $dir/buf"

        printf '%s\n' "eval -draft %{
                  edit! -fifo $dir/fifo *lint-output*
                  set buffer filetype make
                  set buffer make_current_error_line 0
                  hook -group fifo buffer BufCloseFifo .* %{
                      nop %sh{ rm -r '$dir' }
                      remove-hooks buffer fifo
                  }
              }"

        { # do the parsing in the background and when ready send to the session

        eval "$kak_opt_lintcmd '$dir'/buf" | sort -t: -k2,2 -n > "$dir"/stderr
        printf '%s\n' "eval -client $kak_client echo 'linting done'" | kak -p "$kak_session"

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
                errors = errors $2 "," $3 "," substr($4,2) ":"
                # fix case where $5 is not the last field because of extra :s in the message
                for (i=5; i<=NF; i++) errors = errors $i ":"
                errors = substr(errors, 1, length(errors)-1) "\n"
            }
            END {
                print "set \"buffer=" file "\" lint_flags  %{" stamp ":" substr(flags,  1, length(flags)-1)  "}"
                errors = substr(errors, 1, length(errors)-1)
                gsub("~", "\\~", errors)
                print "set \"buffer=" file "\" lint_errors %~" errors "~"
            }
        ' "$dir"/stderr | kak -p "$kak_session"

        cut -d: -f2- "$dir"/stderr | sed "s@^@$kak_bufname:@" > "$dir"/fifo

        } >/dev/null 2>&1 </dev/null &
    }
}

def -hidden lint-show %{ %sh{
    desc=$(printf '%s\n' "$kak_opt_lint_errors" | sed -ne "/^$kak_cursor_line,.*/ { s/^[[:digit:]]*,[[:digit:]]*,//g; s/'/\\\\'/g; p; }")
    if [ -n "$desc" ]; then
        printf '%s\n' "info -anchor $kak_cursor_line.$kak_cursor_column '$desc'"
    fi
}}

def lint-enable -docstring "Activate automatic diagnostics of the code" %{
    add-highlighter flag_lines default lint_flags
    hook window -group lint-diagnostics NormalIdle .* %{ lint-show }
}

def lint-disable -docstring "Disable automatic diagnostics of the code" %{
    remove-highlighter hlflags_lint_flags
    remove-hooks window lint-diagnostics
}

def lint-next -docstring "Jump to the next line that contains an error" %{ %sh{
    printf '%s\n' "$kak_opt_lint_errors" | {
        while read -r line
        do
            # get line,column pair
            coords=$(printf %s "$line" | cut -d, -f1,2)
            candidate="${coords%,*}"
            if [ "$candidate" -gt "$kak_cursor_line" ]
            then
                break
            fi
        done
        if [ "$candidate" -gt "$kak_cursor_line" ]
        then
            col="${coords#*,}"
        else
            candidate="${kak_opt_lint_errors%%,*}"
            col=$(printf '%s\n' "$kak_opt_lint_errors" | head -n1 | cut -d, -f2)
        fi
        printf '%s\n' "select $candidate.$col,$candidate.$col"
    }
}}

def lint-prev -docstring "Jump to the previous line that contains an error" %{ %sh{
    printf '%s\n' "$kak_opt_lint_errors" | sort -t, -k1,1 -rn | {
        while read -r line
        do
            coords=$(printf %s "$line" | cut -d, -f1,2)
            candidate="${coords%,*}"
            if [ "$candidate" -lt "$kak_cursor_line" ]
            then
                break
            fi
        done
        if [ "$candidate" -lt "$kak_cursor_line" ]
        then
            col="${coords#*,}"
        else
            last=$(printf '%s\n' "$kak_opt_lint_errors" | tail -n1)
            candidate=$(printf '%s\n' "$last" | cut -d, -f1)
            col=$(printf '%s\n' "$last" | cut -d, -f2)
        fi
        printf '%s\n' "select $candidate.$col,$candidate.$col"
    }
}}
