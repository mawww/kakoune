## Maximum amount of characters per line
decl int autowrap_column 80

def -hidden _autowrap-break-line %{
    try %{
        ## <a-:><a-;>: ensure that the cursor is located after the anchor, then reverse the
        ##             selection (make sure the cursor is at the beginning of the selection)
        ## <a-k>(?=[^\n]*\h)(?=[^\n]*[^\h])[^\n]{%opt{autowrap_column},}[^\n]<ret>: make sure
        ##             the selection is wrap-able (contains at least an horizontal space, any
        ##             non-whitespace character, and at least "autowrap_column" characters)
        ## %opt{autowrap_column}l: place the cursor on the "autowrap_column"th character
        ## <a-i>w<a-;>;i<ret><esc>: move the cursor to the beginning of the word (if it overlaps
        ##             the "autowrap_column"th column), or do nothing if the "autowrap_column"th
        ##             character is a horizontal space, and insert a newline
        ## <a-x>:_autowrap-break-line: select the second half of the buffer that was just split,
        ##             and call itself recursively until all lines in the selection
        ##             are correctly split
        exec -draft "<a-:><a-;>
                    <a-k>(?=[^\n]*\h)(?=[^\n]*[^\h])[^\n]{%opt{autowrap_column},}[^\n]<ret>
                    %opt{autowrap_column}l
                    <a-i>w<a-;>;i<ret><esc>
                    <a-x>:_autowrap-break-line<ret>"
    }
}

## Automatically wrap the selection
def autowrap-selection -docstring "Wrap the selection" %{
    eval -draft _autowrap-break-line

    try %{
        exec -draft "<a-k>^[^\n]*\h*\n\h*[^\n]+$<ret>
                    s\h*\n\h*(?=[^\z])<ret>c<ret><esc>"
    }
}

## Add a hook that will wrap the entire line every time a key is pressed
def autowrap-enable -docstring "Wrap the lines in which characters are inserted" %{
    hook -group autowrap window InsertChar [^\n] %{
        exec -draft "<a-x>:autowrap-selection<ret>"
    }
}

## Disable the automatic line wrapping, autowrap-selection remains available though
def autowrap-disable -docstring "Disable automatic line wrapping" %{
    rmhooks window autowrap
}
