decl str clang_filename
decl str clang_options

def clang-complete %{
    %sh{
        filename=$(mktemp -d -t kak-clang.XXXXXXXX)/buffer.cpp
        echo "setb clang_filename $filename"
        echo "write $filename"
    }
    # end the previous %sh{} so that its output gets interpreted by kakoune
    # before launching the following as a background task.
    %sh{
        # this runs in a detached shell, asynchronously, so that kakoune does not hang while clang is running.
        # As completions references a cursor position and a buffer timestamp, only valid completions should be
        # displayed.
        (
            pos=-:${kak_cursor_line}:${kak_cursor_column}
            cd $(dirname ${kak_bufname})
            output=$(clang++ -x c++ -fsyntax-only ${kak_opt_clang_options} -Xclang -code-completion-at=${pos} - < ${kak_opt_clang_filename} |
                     grep -E "^COMPLETION:[^:]+:" | perl -pe 's/^COMPLETION:[^:]+: +//; s/:/\\:/g; s/\[#.*?#\]|<#.*?#>(, *|\))?|\{#.*?#\}\)?//g')
            rm -r $(dirname ${kak_opt_clang_filename})
            completions="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"
            for cmp in ${output}; do
                completions="${completions}:${cmp}"
            done
            echo "eval -client $kak_client %[ echo completed; setb completions '${completions}' ]" | socat -u stdin UNIX-CONNECT:${kak_socket}
        ) >& /dev/null < /dev/null &
    }
}

def clang-enable-autocomplete %{
    hook window -id clang-autocomplete InsertIdle .* %{ eval -draft %{
        exec <a-h>
        %sh{ [[ $kak_selection =~ .*(\.|->|::).$ ]] && echo "exec <a-space>; echo 'completing...'; clang-complete" }
    }}
}

def clang-disable-autocomplete %{ rmhooks window clang-autocomplete }
