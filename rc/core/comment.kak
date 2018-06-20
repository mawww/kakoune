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

hook global BufSetOption filetype=(c|cpp|go|java|javascript|objc|php|rust|sass|scala|scss|swift|typescript) %{
    set-option buffer comment_line '//'
    set-option buffer comment_block_begin '/*'
    set-option buffer comment_block_end '*/'
}

hook global BufSetOption filetype=(cabal|haskell|moon|idris) %{
    set-option buffer comment_line '--'
    set-option buffer comment_block_begin '{-'
    set-option buffer comment_block_end '-}'
}

hook global BufSetOption filetype=clojure %{
    set-option buffer comment_line '#_'
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

hook global BufSetOption filetype=(html|xml) %{
    set-option buffer comment_line ''
    set-option buffer comment_block_begin '<!--'
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
    evaluate-commands %sh{
        if [ -z "${kak_opt_comment_block_begin}" ] || [ -z "${kak_opt_comment_block_end}" ]; then
            echo "fail \"The 'comment_block' options are empty, could not comment the selection\""
        fi
    }
    evaluate-commands -draft %{
        # Keep non-empty selections
        execute-keys <a-K>\A\s*\z<ret>

        try %{
            # Assert that the selection has been commented
            set-register / "\A\Q%opt{comment_block_begin}\E.*\Q%opt{comment_block_end}\E\n*\z"
            execute-keys "s<ret>"
            # Uncomment it
            set-register / "\A\Q%opt{comment_block_begin}\E|\Q%opt{comment_block_end}\E\n*\z"
            execute-keys s<ret>d
        } catch %{
            # Comment the selection
            set-register '"' "%opt{comment_block_begin}"
            execute-keys P
            set-register '"' "%opt{comment_block_end}"
            execute-keys p
        }
    }
}

define-command comment-line -docstring '(un)comment selected lines using line comments' %{
    evaluate-commands %sh{
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
                # Select the comment characters and remove them
                set-register / "\A\Q%opt{comment_line}\E\h*"
                execute-keys s<ret>d
            } catch %{
                # Comment the line
                set-register '"' "%opt{comment_line} "
                execute-keys P
            }
        }
    }
}
