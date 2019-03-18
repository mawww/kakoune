declare-option -hidden str jedi_tmp_dir
declare-option -hidden completions jedi_completions
declare-option -docstring "colon separated list of path added to `python`'s $PYTHONPATH environment variable" \
    str jedi_python_path

define-command jedi-complete -docstring "Complete the current selection" %{
    evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-jedi.XXXXXXXX)
        mkfifo ${dir}/fifo
        printf %s\\n "set-option buffer jedi_tmp_dir ${dir}"
        printf %s\\n "evaluate-commands -no-hooks write -sync ${dir}/buf"
    }
    evaluate-commands %sh{
        dir=${kak_opt_jedi_tmp_dir}
        printf %s\\n "evaluate-commands -draft %{ edit! -fifo ${dir}/fifo *jedi-output* }"
        (
            cd $(dirname ${kak_buffile})
            header="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"

            export PYTHONPATH="$kak_opt_jedi_python_path:$PYTHONPATH" 
            compl=$(python 2> "${dir}/fifo" <<-END
		import jedi
		script=jedi.Script(open('$dir/buf', 'r').read(), $kak_cursor_line, $kak_cursor_column - 1, '$kak_buffile')
		print(' '.join(["'" + (str(c.name).replace("|", "\\|") + "|" + str(c.docstring()).replace("|", "\\|") + "|" + str(c.name).replace("|", "\\|")).replace("~", "~~").replace("'", "''") + "'" for c in script.completions()]))
		END
            )
            printf %s\\n "evaluate-commands -client ${kak_client} %~echo completed; set-option %{buffer=${kak_buffile}} jedi_completions ${header} ${compl}~" | kak -p ${kak_session}
            rm -r ${dir}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

define-command jedi-enable-autocomplete -docstring "Add jedi completion candidates to the completer" %{
    set-option window completers option=jedi_completions %opt{completers}
    hook window -group jedi-autocomplete InsertIdle .* %{ try %{
        execute-keys -draft <a-h><a-k>\..\z<ret>
        echo 'completing...'
        jedi-complete
    } }
    alias window complete jedi-complete
}

define-command jedi-disable-autocomplete -docstring "Disable jedi completion" %{
    set-option window completers %sh{ printf %s\\n "'${kak_opt_completers}'" | sed -e 's/option=jedi_completions://g' }
    remove-hooks window jedi-autocomplete
    unalias window complete jedi-complete
}
