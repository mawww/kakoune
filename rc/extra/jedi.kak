decl -hidden str jedi_tmp_dir
decl -hidden completions jedi_completions
decl str-list jedi_python_path ''

def jedi-complete -docstring "Complete the current selection" %{
    %sh{
        dir=$(mktemp -d -t kak-jedi.XXXXXXXX)
        mkfifo ${dir}/fifo
        printf %s\\n "set buffer jedi_tmp_dir ${dir}"
        printf %s\\n "eval -no-hooks write ${dir}/buf"
    }
    %sh{
        dir=${kak_opt_jedi_tmp_dir}
        printf %s\\n "eval -draft %{ edit! -fifo ${dir}/fifo *jedi-output* }"
        (
            cd $(dirname ${kak_buffile})
            header="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"

            export PYTHONPATH="$kak_opt_jedi_python_path:$PYTHONPATH" 
            compl=$(python 2> "${dir}/fifo" <<-END
		import jedi
		script=jedi.Script(open('$dir/buf', 'r').read(), $kak_cursor_line, $kak_cursor_column - 1, '$kak_buffile')
		print ':'.join([(str(c.name).replace("|", "\\|") + "|" + str(c.docstring()).replace("|", "\\|")).replace(":", "\\:") + "|" + str(c.name).replace("|", "\\|") for c in script.completions()]).replace("'", r"\\\\'")
		END
            )
            printf %s\\n "${compl}" > /tmp/kak-jedi-out
            printf %s\\n "eval -client ${kak_client} 'echo completed; set %{buffer=${kak_buffile}} jedi_completions \'${header}:${compl}\''" | kak -p ${kak_session}
            rm -r ${dir}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def jedi-enable-autocomplete -docstring "Add jedi completion candidates to the completer" %{
    set window completers "option=jedi_completions:%opt{completers}"
    hook window -group jedi-autocomplete InsertIdle .* %{ try %{
        exec -draft <a-h><a-k>\..\'<ret>
        echo 'completing...'
        jedi-complete
    } }
    alias window complete jedi-complete
}

def jedi-disable-autocomplete -docstring "Disable jedi completion" %{
    set window completers %sh{ printf %s\\n "'${kak_opt_completers}'" | sed -e 's/option=jedi_completions://g' }
    remove-hooks window jedi-autocomplete
    unalias window complete jedi-complete
}
