decl str flake8_options
decl -hidden str flake8_tmp_dir
decl -hidden line-flags flake8_flags
decl -hidden str flake8_errors

def flake8-lint -params 0..1 -docstring "Lint the contents of the current buffer with flake8" %{
    %sh{
        dir=$(mktemp -d -t kak-flake8.XXXXXXXX)
        mkfifo ${dir}/fifo
        echo "set buffer flake8_tmp_dir ${dir}"
        echo "write ${dir}/buf"
    }
    # end the previous %sh{} so that its output gets interpreted by kakoune
    # before launching the following as a background task.
    %sh{
        dir=${kak_opt_flake8_tmp_dir}
        echo "eval -draft %{
                  edit! -fifo ${dir}/fifo *flake8-output*
                  set buffer filetype make
                  set buffer _make_current_error_line 0
                  hook -group fifo buffer BufCloseFifo .* %{
                      nop %sh{ rm -r ${dir} }
                      rmhooks buffer fifo
                  }
              }"
        # this runs in a detached shell, asynchronously, so that kakoune does
        # not hang while flake8 is running.
		(
			flake8 --ignore=${kak_opt_flake8_options} - < ${dir}/buf > ${dir}/stderr

            flags=$(cat ${dir}/stderr | sed -rne "
						/^stdin:[0-9]+:[0-9]+:? (W|F|C|N).*/ { s/^stdin:([0-9]+):.*/\1|{yellow}█/; p }
						/^stdin:[0-9]+:[0-9]+:? E.*/ { s/^stdin:([0-9]+):.*/\1|{red}█/; p }
                    " | paste -s -d ':')

            errors=$(cat ${dir}/stderr | sed -rne "
						/^stdin:[0-9]+:[0-9]+:?.*/ { s/^stdin:([0-9]+):([0-9]+:)? (.*) /\1,\3/; s/'/\\\\'/g; p }
                     " | sort -n)

            sed -e "s|<stdin>|${kak_bufname}|g" < ${dir}/stderr > ${dir}/fifo

            echo "set 'buffer=${kak_buffile}' flake8_flags %{${kak_timestamp}:${flags}}
                  set 'buffer=${kak_buffile}' flake8_errors '${errors}'" | kak -p ${kak_session}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def -hidden flake8-show-error-info %{ %sh{
    desc=$(printf %s "${kak_opt_flake8_errors}" | sed -ne "/^${kak_cursor_line},.*/ { s/^[[:digit:]]\+,//g; s/'/\\\\'/g; p }")
    if [ -n "$desc" ]; then
        echo "info -anchor ${kak_cursor_line}.${kak_cursor_column} '${desc}'"
    fi
} }

def flake8-enable-diagnostics -docstring "Activate automatic diagnostics of the code by flake8" %{
    addhl flag_lines default flake8_flags
    hook window -group flake8-diagnostics NormalIdle .* %{ flake8-show-error-info }
}

def flake8-disable-diagnostics -docstring "Disable automatic diagnostics of the code" %{
    rmhl hlflags_flake8_flags
    rmhooks window flake8-diagnostics
}

def flake8-diagnostics-next -docstring "Jump to the next line that contains an error" %{ %sh{
    printf "%s\n" "${kak_opt_flake8_errors}" | (
        line=-1
        first_line=-1
        while read line_content; do
            candidate=${line_content%%,*}
            if [ -n "$candidate" ]; then
                first_line=$(( first_line == -1 ? candidate : first_line ))
                line=$((candidate > kak_cursor_line && (candidate < line || line == -1) ? candidate : line ))
            fi
        done
        line=$((line == -1 ? first_line : line))
        if [ ${line} -ne -1 ]; then
            echo "exec ${line} g"
        else
            echo 'echo -color Error no next flake8 diagnostic'
        fi
    )
} }
