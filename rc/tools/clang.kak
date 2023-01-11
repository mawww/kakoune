hook -once global BufSetOption filetype=(c|cpp) %{
    require-module clang
}

provide-module clang %[

declare-option -docstring "options to pass to the `clang` shell command" \
    str clang_options

declare-option -docstring "directory from which to invoke clang" \
    str clang_directory

declare-option -hidden completions clang_completions
declare-option -hidden line-specs clang_flags
declare-option -hidden line-specs clang_errors

define-command -params ..1 \
    -docstring %{
        Parse the contents of the current buffer
        The syntaxic errors detected during parsing are shown when auto-diagnostics are enabled
    } clang-parse %{
    evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-clang.XXXXXXXX)
        mkfifo ${dir}/fifo
        printf %s\\n "
            evaluate-commands -no-hooks write -sync -method replace ${dir}/buf
            evaluate-commands -draft %{
                edit! -fifo ${dir}/fifo -debug *clang-output*
                set-option buffer filetype make
                set-option buffer make_current_error_line 0
                hook -once -always buffer BufCloseFifo .* %{ nop %sh{ rm -r ${dir} } }
            }"

        # this runs in a detached shell, asynchronously, so that kakoune does
        # not hang while clang is running. As completions references a cursor
        # position and a buffer timestamp, only valid completions should be
        # displayed.
        ((
            until [ -f ${dir}/buf ]; do :; done # wait for the buffer to be written

            if [ -n "$kak_opt_clang_directory" ]; then
                cd "$kak_opt_clang_directory"
            fi
            case ${kak_opt_filetype} in
                c) ft=c ;;
                cpp) ft=c++ ;;
                obj-c) ft=objective-c ;;
                *) ft=c++ ;;
            esac

            if [ "$1" = "-complete" ]; then
                pos=-:${kak_cursor_line}:${kak_cursor_column}
                header="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"
                compl=$(clang++ -x ${ft} -fsyntax-only ${kak_opt_clang_options} \
                    -Xclang -code-completion-brief-comments -Xclang -code-completion-at=${pos} - < ${dir}/buf 2> ${dir}/stderr |
                        awk -F ': ' '
                            /^COMPLETION:/ && $2 !~ /[(,](Hidden|Inaccessible)[),]/ {
                                 candidate=$3
                                 gsub(/[[<{]#[^#]*#[]>}]/, "", candidate)
                                 gsub(/~/, "~~", candidate)
                                 gsub(/\|/, "\\|", candidate)

                                 gsub(/[[{<]#|#[]}>]/, " ", $3)
                                 gsub(/:: /, "::", $3)
                                 gsub(/ ,/, ",", $3)
                                 gsub(/^ +| +$/, "", $3)
                                 docstring=$4 ? $3 "\n" $4 : $3

                                 gsub(/~|!/, "&&", docstring)
                                 gsub(/\|/, "\\|", docstring)
                                 if (candidate in candidates)
                                     candidates[candidate]=candidates[candidate] "\n" docstring
                                 else
                                     candidates[candidate]=docstring
                            }
                            END {
                                for (candidate in candidates) {
                                    menu=candidate
                                    gsub(/(^|[^[:alnum:]_])(operator|new|delete)($|[^{}_[:alnum:]]+)/, "{keyword}&{}", menu)
                                    gsub(/(^|[[:space:]])(int|size_t|bool|char|unsigned|signed|long)($|[[:space:]])/, "{type}&{}", menu)
                                    gsub(/[^{}_[:alnum:]]+/, "{operator}&{}", menu)
                                    printf "%%~%s|info -style menu %!%s!|%s~ ", candidate, candidates[candidate], menu
                                }
                            }')
                printf %s\\n "evaluate-commands -client ${kak_client} echo 'clang completion done'
                      set-option 'buffer=${kak_buffile}' clang_completions ${header} ${compl}" | kak -p ${kak_session}
            else
                clang++ -x ${ft} -fsyntax-only ${kak_opt_clang_options} - < ${dir}/buf 2> ${dir}/stderr
                printf %s\\n "evaluate-commands -client ${kak_client} echo 'clang parsing done'" | kak -p ${kak_session}
            fi

            flags=$(cat ${dir}/stderr | sed -Ene "
                        /^<stdin>:[0-9]+:([0-9]+:)? (fatal )?error/ { s/^<stdin>:([0-9]+):.*/'\1|{red}█'/; p }
                        /^<stdin>:[0-9]+:([0-9]+:)? warning/ { s/^<stdin>:([0-9]+):.*/'\1|{yellow}█'/; p }
                    " | paste -s -d ' ' -)

            errors=$(cat ${dir}/stderr | sed -Ene "
                        /^<stdin>:[0-9]+:([0-9]+:)? ((fatal )?error|warning)/ {
                            s/'/''/g; s/^<stdin>:([0-9]+):([0-9]+:)? (.*)/'\1|\3'/; p
                        }" | sort -n | paste -s -d ' ' -)

            sed -e "s|<stdin>|${kak_bufname}|g" < ${dir}/stderr > ${dir}/fifo

            printf %s\\n "set-option 'buffer=${kak_buffile}' clang_flags ${kak_timestamp} ${flags}
                  set-option 'buffer=${kak_buffile}' clang_errors ${kak_timestamp} ${errors}" | kak -p ${kak_session}
        ) & ) > /dev/null 2>&1 < /dev/null
    }
}

define-command clang-complete -docstring "Complete the current selection" %{ clang-parse -complete }

define-command -hidden clang-show-completion-info %[ try %[
    evaluate-commands -draft %[
        execute-keys ,{( <a-k> ^\( <ret> b <a-k> \A\w+\z <ret>
        evaluate-commands %sh[
            desc=$(printf %s\\n "${kak_opt_clang_completions}" | sed -e "{ s/\([^\\]\):/\1\n/g }" | sed -ne "/^${kak_selection}|/ { s/^[^|]\+|//; s/|.*$//; s/\\\:/:/g; p }")
            if [ -n "$desc" ]; then
                printf %s\\n "evaluate-commands -client $kak_client %{info -anchor ${kak_cursor_line}.${kak_cursor_column} -style above %{${desc}}}"
            fi
    ] ]
] ]

define-command clang-enable-autocomplete -docstring "Enable automatic clang completion" %{
    set-option window completers "option=clang_completions" %opt{completers}
    hook window -group clang-autocomplete InsertIdle .* %{
        try %{
            execute-keys -draft <a-h><a-k>(\.|->|::).\z<ret>
            echo 'completing...'
            clang-complete
        }
        clang-show-completion-info
    }
    alias window complete clang-complete
}

define-command clang-disable-autocomplete -docstring "Disable automatic clang completion" %{
    evaluate-commands %sh{ printf "set-option window completers %s\n" $(printf %s "${kak_opt_completers}" | sed -e "s/'option=clang_completions'//g") }
    remove-hooks window clang-autocomplete
    unalias window complete clang-complete
}

define-command -hidden clang-show-error-info %{
    update-option buffer clang_errors # Ensure we are up to date with buffer changes
    evaluate-commands %sh{
        eval "set -- ${kak_quoted_opt_clang_errors}"
        shift # skip timestamp
        desc=$(for error in "$@"; do
            if [ "${error%%|*}" = "$kak_cursor_line" ]; then
                printf '%s\n' "${error##*|}"
            fi
        done)
        if [ -n "$desc" ]; then
            desc=$(printf %s "${desc}" | sed "s/'/''/g")
            printf "info -anchor %d.%d '%s'\n" "${kak_cursor_line}" "${kak_cursor_column}" "${desc}"
        fi
    } }

define-command clang-enable-diagnostics -docstring %{
    Activate automatic error reporting and diagnostics
    Information about the analysis will be shown after the buffer has been parsed with the clang-parse function
} %{
    add-highlighter window/clang_flags flag-lines default clang_flags
    hook window -group clang-diagnostics NormalIdle .* %{ clang-show-error-info }
    hook window -group clang-diagnostics WinSetOption ^clang_errors=.* %{ info; clang-show-error-info }
}

define-command clang-disable-diagnostics -docstring "Disable automatic error reporting and diagnostics" %{
    remove-highlighter window/clang_flags
    remove-hooks window clang-diagnostics
}

define-command clang-diagnostics-next -docstring "Jump to the next line that contains an error" %{
    update-option buffer clang_errors # Ensure we are up to date with buffer changes
    evaluate-commands %sh{
        eval "set -- ${kak_quoted_opt_clang_errors}"
        shift # skip timestamp
        unset line
        unset first_line
        for error in "$@"; do
            candidate=${error%%|*}
            first_line=${first_line-$candidate}
            if [ "$candidate" -gt $kak_cursor_line ]; then
                line=$candidate
                break
            fi
        done
        line=${line-$first_line}
        if [ -n "$line" ]; then
            printf %s\\n "execute-keys ${line} g"
        else
            echo "fail no next clang diagnostic"
        fi
    } }

]
