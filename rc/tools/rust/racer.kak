hook -once global BufSetOption filetype=rust %{
    require-module racer
}

provide-module racer %{

declare-option -hidden str racer_tmp_dir
declare-option -hidden completions racer_completions

define-command racer-complete -docstring "Complete the current selection with racer" %{
    evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-racer.XXXXXXXX)
        printf %s\\n "set-option buffer racer_tmp_dir ${dir}"
        printf %s\\n "evaluate-commands -no-hooks %{ write ${dir}/buf }"
    }
    evaluate-commands %sh{
        dir=${kak_opt_racer_tmp_dir}
        (
            cursor="${kak_cursor_line} $((${kak_cursor_column} - 1))"
            racer_data=$(racer --interface tab-text complete-with-snippet ${cursor} ${kak_buffile} ${dir}/buf)
            compl=$(printf %s\\n "${racer_data}" | awk '
                BEGIN { FS = "\t"; ORS = " " }
                /^PREFIX/ {
                    column = ENVIRON["kak_cursor_column"] + $2 - $3
                    print ENVIRON["kak_cursor_line"] "." column "@@" ENVIRON["kak_timestamp"]
                }
                /^MATCH/ {
                    word = $2
                    desc = substr($9, 2, length($9) - 2)
                    gsub(/\|/, "\\|", desc)
                    gsub(/\\n/, "\n", desc)
                    gsub(/!/, "!!", desc)
                    info = $8
                    gsub(/\|/, "\\|", info)

                    candidate = word "|info -style menu %!" desc "!|" word " {MenuInfo}" info

                    gsub(/@/, "@@", candidate)
                    gsub(/~/, "~~", candidate)
                    print "%~" candidate "~"
                }'
            )
            printf %s\\n "evaluate-commands -client '${kak_client}' %@ set-option 'buffer=${kak_bufname}' racer_completions ${compl%?} @" | kak -p ${kak_session}
            rm -r ${dir}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

define-command racer-enable-autocomplete -docstring "Add racer completion candidates to the completer" %{
    set-option window completers option=racer_completions %opt{completers}
    hook window -group racer-autocomplete InsertIdle .* %{ try %{
        execute-keys -draft <a-h><a-k>([\w\.]|::).\z<ret>
        racer-complete
    } }
    alias window complete racer-complete
}

define-command racer-disable-autocomplete -docstring "Disable racer completion" %{
    evaluate-commands %sh{ printf "set-option window completers %s\n" $(printf %s "${kak_opt_completers}" | sed -e "s/'option=racer_completions'//g") }
    remove-hooks window racer-autocomplete
    unalias window complete racer-complete
}

define-command racer-go-definition -docstring "Jump to where the rust identifier below the cursor is defined" %{
    evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-racer.XXXXXXXX)
        printf %s\\n "set-option buffer racer_tmp_dir ${dir}"
        printf %s\\n "evaluate-commands -no-hooks %{ write ${dir}/buf }"
    }
    evaluate-commands %sh{
        dir=${kak_opt_racer_tmp_dir}
        cursor="${kak_cursor_line} $((${kak_cursor_column} - 1))"
        racer_data=$(racer --interface tab-text  find-definition ${cursor} "${kak_buffile}" "${dir}/buf" | head -n 1)

        racer_match=$(printf %s\\n "$racer_data" | cut -f1 )
        if [ "$racer_match" = "MATCH" ]; then
          racer_line=$(printf %s\\n "$racer_data" | cut -f3 )
          racer_column=$(printf %s\\n "$racer_data" | cut -f4 )
          racer_file=$(printf %s\\n "$racer_data" | cut -f5 )
          printf %s\\n "edit -existing '$racer_file' $racer_line $racer_column"
          case ${racer_file} in
            "${RUST_SRC_PATH}"* | "${CARGO_HOME:-$HOME/.cargo}"/registry/src/*)
              printf %s\\n "set-option buffer readonly true";;
          esac
        else
          printf %s\\n "echo -debug 'racer could not find a definition'"
        fi
    }
}

define-command racer-show-doc -docstring "Show the documentation about the rust identifier below the cursor" %{
    evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-racer.XXXXXXXX)
        printf %s\\n "set-option buffer racer_tmp_dir ${dir}"
        printf %s\\n "evaluate-commands -no-hooks %{ write ${dir}/buf }"
    }
    evaluate-commands %sh{
        dir=${kak_opt_racer_tmp_dir}
        cursor="${kak_cursor_line} ${kak_cursor_column}"
        racer_data=$(racer --interface tab-text  complete-with-snippet  ${cursor} "${kak_buffile}" "${dir}/buf" | sed -n 2p )
        racer_match=$(printf %s\\n "$racer_data" | cut -f1)
        if [ "$racer_match" = "MATCH" ]; then
          racer_doc=$(
            printf %s\\n "$racer_data" |
            cut -f9  |
            sed -e '

              # Remove leading and trailing quotes
              s/^"\(.*\)"$/\1/g

              # Escape all @ so that it can be properly used in the string expansion
              s/@/\\@/g

            ')
          printf "info %%@$racer_doc@"
        else
          printf %s\\n "echo -debug 'racer could not find a definition'"
        fi
    }
}

}
