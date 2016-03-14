## Maximum amount of characters per line
decl int autowrap_column 80

# remove the whitespaces selected by `autowrap-line`
# and handle "recursive" calls when needed
def -hidden _autowrap-cut-selection %{
    try %{
        # remove the whitespaces
        # then save the indentation of the original line and apply it to the new one
        exec -draft c<ret><esc> K <a-&>
        # if there's text after the current line, merge the two
        exec xX <a-k>[^\n]\n[^\n]<ret> <a-j>
    }
    eval autowrap-line
}

def autowrap-line -docstring "Wrap the current line" %{ eval -draft %{
    try %{
        # check that the line is too long and has to be wrapped
        exec "<a-x> s^(?=.*\h).{%opt{autowrap_column}}[^\n]<ret>"
        try %{
            # either select the trailing whitespaces, or the ones before the last word of the line
            exec s\h+(?=[^\h]+\')|\h+\'<ret>
            eval _autowrap-cut-selection
        } catch %{ try %{
            # wrap the line on the first whitespace that's past the column limit
            exec "x s^[^\h]{%opt{autowrap_column},}\K\h+<ret>"
            eval _autowrap-cut-selection
        } }
    }
} }

def autowrap-enable -docstring "Wrap the lines in which characters are inserted" %{
    hook -group autowrap window InsertChar [^\n] %{
        eval autowrap-line
    }
}

def autowrap-disable -docstring "Disable automatic line wrapping" %{
    rmhooks window autowrap
}
