hook -once global BufSetOption filetype=python %{
    require-module jedi
}

provide-module jedi %{

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
        ((
            cd $(dirname ${kak_buffile})

            export PYTHONPATH="$kak_opt_jedi_python_path:$PYTHONPATH"
            python 2> "${dir}/fifo" -c 'if 1:
                import os
                dir = os.environ["kak_opt_jedi_tmp_dir"]
                buffile = os.environ["kak_buffile"]
                line = int(os.environ["kak_cursor_line"])
                column = int(os.environ["kak_cursor_column"])
                timestamp = os.environ["kak_timestamp"]
                client = os.environ["kak_client"]
                pipe_escape = lambda s: s.replace("|", "\\|")
                def quote(s):
                    c = chr(39) # single quote
                    return c + s.replace(c, c+c) + c
                import jedi
                script = jedi.Script(code=open(dir + "/buf", "r").read(), path=buffile)
                completions = (
                    quote(
                        pipe_escape(str(c.name)) + "|" +
                        pipe_escape("info -style menu -- " + quote(c.docstring())) + "|" +
                        pipe_escape(str(c.name))
                    )
                    for c in script.complete(line=line, column=column-1)
                )
                header = str(line) + "." + str(column) + "@" + timestamp
                cmds = [
                    "echo completed",
                    " ".join(("set-option", quote("buffer=" + buffile), "jedi_completions", header, *completions)),
                ]
                print("evaluate-commands -client", quote(client), quote("\n".join(cmds)))
            ' | kak -p "${kak_session}"
            rm -r ${dir}
        ) & ) > /dev/null 2>&1 < /dev/null
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

}
