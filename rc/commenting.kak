## Characters that will be used to surround a selection with
decl str-list comment_selection_chars "/*:*/"

## Characters that will be inserted at the beginning of a line to comment
decl str comment_line_chars "//"

def comment-selection -docstring "Comment/uncomment the current selection" %{
    %sh{
        function exec_proof {
            ## Replace the '<' sign that is interpreted differently in `exec`
            echo "$@" | sed -r 's,<,<lt>,g'
        }

        readonly opening=$(exec_proof "${kak_opt_comment_selection_chars%%:*}")
        readonly closing=$(exec_proof "${kak_opt_comment_selection_chars##*:}")

        if [ -z "${opening}" -o -z "${closing}" ]; then
            echo "The \`comment_selection_chars\` variable is empty, couldn't comment the selection" >&2
            exit
        fi

        echo "try %{
            ## The selection is empty
            exec -draft %{<a-K>\A[\h\v\n]*\z<ret>}

            try %{
                ## The selection has already been commented
                exec -draft %{<a-K>\A\Q${opening}\E.*\Q${closing}\E\z<ret>}

                ## Comment the selection
                exec %{a${closing}<esc>i${opening}<esc>${#opening}H}
            } catch %{
                ## Uncomment the commented selection
                exec -draft %{s(\A\Q${opening}\E)|(\Q${closing}\E\z)<ret>d}
            }
        }"
    }
}

def comment-line -docstring "Comment/uncomment the current line" %{
    %sh{
        readonly opening="${kak_opt_comment_line_chars}"
        readonly opening_escaped="\Q${opening}\E"

        if [ -z "${opening}" ]; then
            echo "The \`comment_line_chars\` variable is empty, couldn't comment the line" >&2
            exit
        fi

        echo "
        ## Select the content of the line, without indentation
        exec %{I<esc><a-l>}

        try %{
            ## There's no text on the line
            exec -draft %{<a-K>\A[\h\v\n]*\z<ret>}

            try %{
                ## The line has already been commented
                exec -draft %{<a-K>^${opening_escaped}<ret>}

                ## Comment the line
                exec %{i${opening}<esc>${#opening}H}
            } catch %{
                ## Uncomment the line
                exec -draft %{s^${opening_escaped}\h*<ret>d}
            }
        }"
    }
}
