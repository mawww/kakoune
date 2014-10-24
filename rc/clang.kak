decl str clang_options

decl -hidden str clang_tmp_dir
decl -hidden str-list clang_completions

def clang-complete %{
    %sh{
        dir=$(mktemp -d -t kak-clang.XXXXXXXX)
        mkfifo ${dir}/fifo
        echo "set buffer clang_tmp_dir ${dir}"
        echo "write ${dir}/buffer.cpp"
    }
    # end the previous %sh{} so that its output gets interpreted by kakoune
    # before launching the following as a background task.
    %sh{
        dir=${kak_opt_clang_tmp_dir}
        echo "eval -draft %{
                  edit! -fifo ${dir}/fifo *clang-output*
                  set buffer filetype make
                  set buffer _make_current_error_line 0
                  hook buffer BufCloseFifo .* %{ nop %sh{ rm -r ${dir} } }
              }"
        # this runs in a detached shell, asynchronously, so that kakoune does not hang while clang is running.
        # As completions references a cursor position and a buffer timestamp, only valid completions should be
        # displayed.
        (
            pos=-:${kak_cursor_line}:${kak_cursor_column}
            cd $(dirname ${kak_buffile})
            clang++ -x c++ -fsyntax-only ${kak_opt_clang_options} -Xclang -code-completion-at=${pos} - < ${dir}/buffer.cpp 2>&1 | awk -e '
              function rmblocks(opening, closing, val, res) {
                  while (match(val, opening)) {
                      res = res substr(val, 1, RSTART-1)
                      val = substr(val, RSTART + RLENGTH)
                      if (match(val, closing))
                          val = substr(val, RSTART + RLENGTH)
                  }
                  return res val
              }
              BEGIN { out = ENVIRON["kak_cursor_line"] "." ENVIRON["kak_cursor_column"] "@" ENVIRON["kak_timestamp"] }
              /^COMPLETION:[^:]+:/ {
                  gsub("^COMPLETION:[^:]+: +", ""); gsub(":", "\\:")
                  c = rmblocks("\\[#", "#\\]", rmblocks("<#", "#>", rmblocks("\\{#", "#\\}", $0)))
                  gsub("\\((, )+\\)", "(", c); gsub("<(, )+>", "<", c)
                  out = out ":" c
              }
              ! /^COMPLETION:[^:]+:/ { print $0 }
              END {
                  cmd = "kak -p " ENVIRON["kak_session"]
                  print "eval -client " ENVIRON["kak_client"] " %[ echo completed; set buffer clang_completions %[" out "] ]" | cmd
              }' 2>&1 > ${dir}/fifo
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def clang-enable-autocomplete %{
    set window completers %sh{ echo "'option=clang_completions:${kak_opt_completers}'" }
    hook window -group clang-autocomplete InsertIdle .* %{ try %{
        exec -draft <a-h><a-k>(\.|->|::).$<ret>
        echo 'completing...'
        clang-complete
    } }
}

def clang-disable-autocomplete %{
    set window completers %sh{ echo "'${kak_opt_completers}'" | sed -e 's/option=clang_completions://g' }
    rmhooks window clang-autocomplete
}
