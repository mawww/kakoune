# Provides integration of the following tools:
# - gocode for code completion (github.com/nsf/gocode)
# - goimports for code formatting on save
# - gogetdoc for documentation display and source jump (needs jq) (github.com/zmb3/gogetdoc)
# Needs the following tools in the path:
# - jq for json deserializaton

hook -once global BufSetOption filetype=go %{
    require-module go-tools
}

provide-module go-tools %{

evaluate-commands %sh{
    for dep in gocode goimports gogetdoc jq; do
        if ! command -v $dep > /dev/null 2>&1; then
            echo "echo -debug %{Dependency unmet: $dep, please install it to use go-tools}"
        fi
    done
}

# Auto-completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

declare-option -hidden str go_complete_tmp_dir
declare-option -hidden completions gocode_completions

define-command go-complete -docstring "Complete the current selection with gocode" %{
    evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-go.XXXXXXXX)
        printf %s\\n "set-option buffer go_complete_tmp_dir ${dir}"
        printf %s\\n "evaluate-commands -no-hooks write ${dir}/buf"
    }
    nop %sh{
        dir=${kak_opt_go_complete_tmp_dir}
        (
            gocode_data=$(gocode -f=godit --in=${dir}/buf autocomplete ${kak_cursor_byte_offset})
            rm -r ${dir}
            column_offset=$(printf %s "${gocode_data}" | head -n1 | cut -d, -f1)

            header="${kak_cursor_line}.$((${kak_cursor_column} - $column_offset))@${kak_timestamp}"
            compl=$(echo "${gocode_data}" | sed 1d | awk -F ",," '{
                        gsub(/~/, "~~", $1)
                        gsub(/~/, "~~", $2)
                        print "%~" $2 "||" $1 "~"
                    }' | paste -s -)
            printf %s\\n "evaluate-commands -client '${kak_client}' %{
                set-option 'buffer=${kak_bufname}' gocode_completions ${header} ${compl}
            }" | kak -p ${kak_session}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

define-command go-enable-autocomplete -docstring "Add gocode completion candidates to the completer" %{
    set-option window completers "option=gocode_completions" %opt{completers}
    hook window -group go-autocomplete InsertIdle .* %{ try %{
        execute-keys -draft <a-h><a-k>[\w\.].\z<ret>
        go-complete
    } }
    alias window complete go-complete
}

define-command go-disable-autocomplete -docstring "Disable gocode completion" %{
    set-option window completers %sh{ printf %s\\n "${kak_opt_completers}" | sed "s/'option=gocode_completions'//g" }
    remove-hooks window go-autocomplete
    unalias window complete go-complete
}

# Auto-format
# ‾‾‾‾‾‾‾‾‾‾‾

declare-option -hidden str go_format_tmp_dir

define-command -params ..1 go-format \
    -docstring "go-format [-use-goimports]: custom formatter for go files" %{
    evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-go.XXXXXXXX)
        printf %s\\n "set-option buffer go_format_tmp_dir ${dir}"
        printf %s\\n "evaluate-commands -no-hooks write ${dir}/buf"
    }
    evaluate-commands %sh{
        dir=${kak_opt_go_format_tmp_dir}
        if [ "$1" = "-use-goimports" ]; then
            fmt_cmd="goimports -srcdir '${kak_buffile}'"
        else
            fmt_cmd="gofmt -s"
        fi
        eval "${fmt_cmd} -e -w ${dir}/buf 2> ${dir}/stderr"
        if [ $? ]; then
            cp ${dir}/buf "${kak_buffile}"
        else
            # we should report error if linting isn't active
            printf %s\\n "echo -debug '$(cat ${dir}/stderr)'"
        fi
        rm -r ${dir}
    }
    edit!
}

# Documentation
# ‾‾‾‾‾‾‾‾‾‾‾‾‾

declare-option -hidden str go_doc_tmp_dir

# FIXME text escaping
define-command -hidden -params 1..2 gogetdoc-cmd %{
   evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-go.XXXXXXXX)
        printf %s\\n "set-option buffer go_doc_tmp_dir ${dir}"
        printf %s\\n "evaluate-commands -no-hooks write ${dir}/buf"
    }
    evaluate-commands %sh{
        dir=${kak_opt_go_doc_tmp_dir}
        (
            printf %s\\n "${kak_buffile}" > ${dir}/modified
            cat ${dir}/buf | wc -c >> ${dir}/modified
            cat ${dir}/buf >> ${dir}/modified

            if [ "$2" = "1" ]; then
                args="-json"
            fi
            output=$(cat ${dir}/modified                                                    \
		| gogetdoc $args -pos "${kak_buffile}:#${kak_cursor_byte_offset}" -modified \
		| sed 's/%/%%/g')
            rm -r ${dir}
            printf %s "${output}" | grep -v -q "^gogetdoc: "
            status=$?

            case "$1" in
                "info")
                    if [ ${status} -eq 0 ]; then
                        printf %s\\n "evaluate-commands -client '${kak_client}' %{
                            info -anchor ${kak_cursor_line}.${kak_cursor_column} %@${output}@
                        }" | kak -p ${kak_session}
                    else
                        msg=$(printf %s "${output}" | cut -d' ' -f2-)
                        printf %s\\n "evaluate-commands -client '${kak_client}' %{
                            echo '${msg}'
                        }" | kak -p ${kak_session}
                    fi
                    ;;
        	"echo")
                    if [ ${status} -eq 0 ]; then
                        signature=$(printf %s "${output}" | sed -n 3p)
                        printf %s\\n "evaluate-commands -client '${kak_client}' %{
                            echo '${signature}'
                        }" | kak -p ${kak_session}
                    fi
                    ;;
        	"jump")
                    if [ ${status} -eq 0 ]; then
                        pos=$(printf %s "${output}" | jq -r .pos)
                        file=$(printf %s "${pos}" | cut -d: -f1)
                        line=$(printf %s "${pos}" | cut -d: -f2)
                        col=$(printf %s "${pos}" | cut -d: -f3)
                        printf %s\\n "evaluate-commands -client '${kak_client}' %{
                            evaluate-commands -try-client '${kak_opt_jumpclient}' edit -existing ${file} ${line} ${col}
                            try %{ focus '${kak_opt_jumpclient}' }
                        }" | kak -p ${kak_session}
                    fi
                    ;;
                *)
                        printf %s\\n "evaluate-commands -client '${kak_client}' %{
                            echo -error %{unkown command '$1'}
                        }" | kak -p ${kak_session}
                    ;;

            esac
        ) > /dev/null 2>&1 < /dev/null &
    }
}

define-command go-doc-info -docstring "Show the documention of the symbol under the cursor" %{
    gogetdoc-cmd "info"
}

define-command go-print-signature -docstring "Print the signature of the symbol under the cursor" %{
    gogetdoc-cmd "echo"
}

define-command go-jump -docstring "Jump to the symbol definition" %{
    gogetdoc-cmd "jump" 1
}

define-command go-share-selection -docstring "Share the selection using the Go Playground" %{ evaluate-commands %sh{
    snippet_id=$(printf %s\\n "${kak_selection}" | curl -s https://play.golang.org/share --data-binary @-)
    printf "echo https://play.golang.org/p/%s" ${snippet_id}
} }

}
