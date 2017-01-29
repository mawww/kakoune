## Line comments
decl str comment_line '#'

## Block comments
decl str-list comment_block

## Default comments for all languages
hook global BufSetOption filetype=asciidoc %{
    set buffer comment_block '///:///'
}

hook global BufSetOption filetype=(c|cpp|go|java|javascript|objc|php|sass|scala|scss|swift) %{
    set buffer comment_line '//'
    set buffer comment_block '/*:*/'
}

hook global BufSetOption filetype=(cabal|haskell|moon) %{
    set buffer comment_line '--'
}

hook global BufSetOption filetype=clojure %{
    set buffer comment_line '#_ '
    set buffer comment_block '(comment :)'
}

hook global BufSetOption filetype=coffee %{
    set buffer comment_block '###:###'
}

hook global BufSetOption filetype=css %{
    set buffer comment_line ''
    set buffer comment_block '/*:*/'
}

hook global BufSetOption filetype=d %{
    set buffer comment_line '//'
    set buffer comment_block '/+:+/'
}

hook global BufSetOption filetype=(gas|ini) %{
    set buffer comment_line ';'
}

hook global BufSetOption filetype=haml %{
    set buffer comment_line '-#'
}

hook global BufSetOption filetype=html %{
    set buffer comment_line ''
    set buffer comment_block '<!--:-->'
}

hook global BufSetOption filetype=latex %{
    set buffer comment_line '%'
}

hook global BufSetOption filetype=lisp %{
    set buffer comment_line ';'
    set buffer comment_block '#|:|#'
}

hook global BufSetOption filetype=lua %{
    set buffer comment_line '--'
    set buffer comment_block '--[[:]]'
}

hook global BufSetOption filetype=markdown %{
    set buffer comment_line ''
    set buffer comment_block '[//]: # (:)'
}

hook global BufSetOption filetype=perl %{
    set buffer comment_block '#[:]'
}

hook global BufSetOption filetype=(pug|rust) %{
    set buffer comment_line '//'
}

hook global BufSetOption filetype=python %{
    set buffer comment_block '\'\'\':\'\'\''
}

hook global BufSetOption filetype=ragel %{
    set buffer comment_line '%%'
    set buffer comment_block '%%{:}%%'
}

hook global BufSetOption filetype=ruby %{
    set buffer comment_block '^begin=:^=end'
}

def comment-block -docstring '(un)comment selected lines using block comments' %{
    %sh{
        exec_proof() {
            ## Replace the '<' sign that is interpreted differently in `exec`
            printf %s\\n "$@" | sed 's,<,<lt>,g'
        }

        readonly opening=$(exec_proof "${kak_opt_comment_block%:*}")
        readonly closing=$(exec_proof "${kak_opt_comment_block##*:}")

        if [ -z "${opening}" ] || [ -z "${closing}" ]; then
            echo "echo -debug 'The \`comment_block\` variable is empty, could not comment the selection'"
            exit
        fi

        printf %s\\n "eval -draft %{ try %{
            ## The selection is empty
            exec <a-K>\\A[\\h\\v\\n]*\\z<ret>

            try %{
                ## The selection has already been commented
                exec %{<a-K>\\A\\Q${opening}\\E.*\\Q${closing}\\E\\n*\\z<ret>}

                ## Comment the selection
                exec -draft %{a${closing}<esc>i${opening}}
            } catch %{
                ## Uncomment the commented selection
                exec -draft %{s(\\A\\Q${opening}\\E)|(\\Q${closing}\\E\\n*\\z)<ret>d}
            }
        } }"
    }
}

def comment-line -docstring '(un)comment selected lines using line comments' %{
    %sh{
        readonly opening="${kak_opt_comment_line}"
        readonly opening_escaped="\\Q${opening}\\E"

        if [ -z "${opening}" ]; then
            echo "echo -debug 'The \`comment_line\` variable is empty, could not comment the line'"
            exit
        fi

        printf %s\\n "eval -draft %{
            ## Select the content of the lines, without indentation
            exec <a-s>I<esc><a-l>

            try %{
                ## There’s no text on the line
                exec <a-K>\\A[\\h\\v\\n]*\\z<ret>

                try %{
                    ## The line has already been commented
                    exec %{<a-K>\\A${opening_escaped}<ret>}

                    ## Comment the line
                    exec -draft %{i${opening}}
                } catch %{
                    ## Uncomment the line
                    exec -draft %{s\\A${opening_escaped}\\h*<ret>d}
                }
            }
        }"
    }
}
