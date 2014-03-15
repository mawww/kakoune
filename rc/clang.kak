decl -hidden str clang_filename
decl str clang_options
decl str-list clang_completions

def clang-complete %{
    %sh{
        filename=$(mktemp -d -t kak-clang.XXXXXXXX)/buffer.cpp
        echo "set buffer clang_filename $filename"
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
            cd $(dirname ${kak_buffile})
            output=$(clang++ -x c++ -fsyntax-only ${kak_opt_clang_options} -Xclang -code-completion-at=${pos} - < ${kak_opt_clang_filename} 2>&1 | tee /tmp/kak-clang-out |
                     grep -E "^COMPLETION:[^:]+:" | perl -pe 's/^COMPLETION:[^:]+: +//; s/:/\\:/g; s/\[#.*?#\]|<#.*?#>(, *|\))?|\{#.*?#\}\)?//g')
            rm -r $(dirname ${kak_opt_clang_filename})
            completions="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"
            for cmp in ${output}; do
                completions="${completions}:${cmp}"
            done
            echo "eval -client $kak_client %[ echo completed; set buffer clang_completions '${completions}' ]" | kak -p ${kak_session}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def clang-enable-autocomplete %{
    set window completers %sh{ echo "'option=clang_completions:${kak_opt_completers}'" }
    hook window -id clang-autocomplete InsertIdle .* %{ try %{
        exec -draft <a-h><a-k>(\.|->|::).$<ret>
        echo 'completing...'
        clang-complete
    } }
}

def clang-disable-autocomplete %{
    set window completers %sh{ echo "'${kak_opt_completers}'" | sed -e 's/option=clang_completions://g' }
    rmhooks window clang-autocomplete
}
