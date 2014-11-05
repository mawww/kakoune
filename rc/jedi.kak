decl -hidden str jedi_tmp_dir
decl -hidden str-list jedi_completions

def jedi-complete %{
    %sh{
        dir=$(mktemp -d -t kak-jedi.XXXXXXXX)
        mkfifo ${dir}/fifo
        echo "set buffer jedi_tmp_dir ${dir}"
        echo "write ${dir}/buf"
    }
    %sh{
        dir=${kak_opt_jedi_tmp_dir}
        echo "eval -draft %{
                  edit! -fifo ${dir}/fifo *jedi-output*
                  hook -group jedi-cleanup buffer BufCloseFifo .* %{
                     nop %sh{ rm -r ${dir} }
                     rmhooks buffer jedi-cleanup
                  }
              }"
        (
            cd $(dirname ${kak_buffile})
            header="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"

            compl=$( (python | 2>&1 tee "${dir}/fifo") <<-END
		import jedi
		script=jedi.Script(open('$dir/buf', 'r').read(), $kak_cursor_line, $kak_cursor_column - 1, '$kak_buffile')
		print ':'.join([str(c.name) for c in script.completions()])
		END
            )

            echo "eval -client ${kak_client} %[ echo completed; set 'buffer=${kak_buffile}' jedi_completions %[${header}:${compl}] ]" | kak -p ${kak_session}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def jedi-enable-autocomplete %{
    set window completers %sh{ echo "'option=jedi_completions:${kak_opt_completers}'" }
    hook window -group jedi-autocomplete InsertIdle .* %{ try %{
        exec -draft <a-h><a-k>\..\'<ret>
        echo 'completing...'
        jedi-complete
    } }
}

def jedi-disable-autocomplete %{
    set window completers %sh{ echo "'${kak_opt_completers}'" | sed -e 's/option=jedi_completions://g' }
    rmhooks window jedi-autocomplete
}
