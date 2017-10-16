# Line comments
declare-option -docstring "characters inserted at the beginning of a commented line" \
    str comment_line '#'

# Block comments
declare-option -docstring "characters inserted before a commented block" \
    str comment_block_begin
declare-option -docstring "characters inserted after a commented block" \
    str comment_block_end

# Default comments for all languages
hook global BufSetOption filetype=asciidoc %{
    set-option buffer comment_block_begin '///'
    set-option buffer comment_block_end '///'
}

hook global BufSetOption filetype=(c|cpp|go|java|javascript|objc|php|rust|sass|scala|scss|swift) %{
    set-option buffer comment_line '//'
    set-option buffer comment_block_begin '/*'
    set-option buffer comment_block_end '*/'
}

hook global BufSetOption filetype=(cabal|haskell|moon) %{
    set-option buffer comment_line '--'
    set-option buffer comment_block_begin '{-'
    set-option buffer comment_block_end '-}'
}

hook global BufSetOption filetype=clojure %{
    set-option buffer comment_line '#_ '
    set-option buffer comment_block_begin '(comment '
    set-option buffer comment_block_end ')'
}

hook global BufSetOption filetype=coffee %{
    set-option buffer comment_block_begin '###'
    set-option buffer comment_block_end '###'
}

hook global BufSetOption filetype=css %{
    set-option buffer comment_line ''
    set-option buffer comment_block_begin '/*'
    set-option buffer comment_block_end '*/'
}

hook global BufSetOption filetype=d %{
    set-option buffer comment_line '//'
    set-option buffer comment_block_begin '/+'
    set-option buffer comment_block_end '+/'
}

hook global BufSetOption filetype=(gas|ini) %{
    set-option buffer comment_line ';'
}

hook global BufSetOption filetype=haml %{
    set-option buffer comment_line '-#'
}

hook global BufSetOption filetype=html %{
    set-option buffer comment_line ''
    set-option buffer comment_block_begin '<lt>!--'
    set-option buffer comment_block_end '-->'
}

hook global BufSetOption filetype=latex %{
    set-option buffer comment_line '%'
}

hook global BufSetOption filetype=lisp %{
    set-option buffer comment_line ';'
    set-option buffer comment_block_begin '#|'
    set-option buffer comment_block_end '|#'
}

hook global BufSetOption filetype=lua %{
    set-option buffer comment_line '--'
    set-option buffer comment_block_begin '--[['
    set-option buffer comment_block_end ']]'
}

hook global BufSetOption filetype=markdown %{
    set-option buffer comment_line ''
    set-option buffer comment_block_begin '[//]'
    set-option buffer comment_block_end '# (:)'
}

hook global BufSetOption filetype=perl %{
    set-option buffer comment_block_begin '#['
    set-option buffer comment_block_end ']'
}

hook global BufSetOption filetype=pug %{
    set-option buffer comment_line '//'
}

hook global BufSetOption filetype=python %{
    set-option buffer comment_block_begin "'''"
    set-option buffer comment_block_end "'''"
}

hook global BufSetOption filetype=ragel %{
    set-option buffer comment_line '%%'
    set-option buffer comment_block_begin '%%{'
    set-option buffer comment_block_end '}%%'
}

hook global BufSetOption filetype=ruby %{
    set-option buffer comment_block_begin '^begin='
    set-option buffer comment_block_end '^=end'
}

define-command comment-block -docstring '(un)comment selections using block comments' %{
    %sh{
        if [ -z "${kak_opt_comment_block_begin}" ] || [ -z "${kak_opt_comment_block_end}" ]; then
            echo "fail \"The 'comment_block' options are empty, could not comment the selection\""
        fi
    }
    evaluate-commands -draft %{
        # Keep non-empty selections
        execute-keys <a-K>\A\s*\z<ret>

        try %{
            # Assert that the selection has not been commented
            execute-keys "<a-K>\A\Q%opt{comment_block_begin}\E.*\Q%opt{comment_block_end}\E\n*\z<ret>"

            # Comment the selection
            execute-keys -draft "a%opt{comment_block_end}<esc>i%opt{comment_block_begin}"
        } catch %{
            # Uncomment the commented selection
            execute-keys -draft "s(\A\Q%opt{comment_block_begin}\E)|(\Q%opt{comment_block_end}\E\n*\z)<ret>d"
        }
    }
}

define-command comment-line -docstring '(un)comment selected lines using line comments' %{
    %sh{
        if [ -z "${kak_opt_comment_line}" ]; then
            echo "fail \"The 'comment_line' option is empty, could not comment the line\""
        fi
    }
    evaluate-commands -draft %{
        # Select the content of the lines, without indentation
        execute-keys <a-s>gi<a-l>

        try %{
            # Keep non-empty lines
            execute-keys <a-K>\A\s*\z<ret>

            try %{
                # Assert that the line has not been commented
                execute-keys "<a-K>\A\Q%opt{comment_line}\E<ret>"

                # Comment the line
                execute-keys -draft "i%opt{comment_line}"
            } catch %{
                # Uncomment the line
                execute-keys -draft "s\A\Q%opt{comment_line}\E\h*<ret>d"
            }
        }
    }
}
