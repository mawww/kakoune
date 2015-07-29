## Maximum amount of characters per line
decl int autowrap_column 80

## Automatically wrap the selection
def autowrap-selection %{
    try %{
        ## <a-:><a-;>: ensure that the cursor is located after the anchor, then reverse the
        ##             selection (make sure the cursor is at the beginning of the selection)
        ## <a-k>(?=[^\n]*\h)(?=[^\n]*[^\h])[^\n]{%opt{autowrap_column},}[^\n]<ret>: make sure
        ##             the selection is wrap-able (contains at least an horizontal space, any
        ##             non-whitespace character, and at least "autowrap_column" characters)
        ## %opt{autowrap_column}l: place the cursor on the "autowrap_column"th character
        ## <a-i>w<a-;>;i<ret><esc>: move the cursor to the beginning of the word (if it overslaps
        ##             the "autowrap_column"th column), or do nothing if the "autowrap_column"th
        ##             character is a horizontal space, and insert a newline
        ## kxXs\h+$<ret>d : select the line that we just made, as well as the one that was just
        ##             wrapped, and remove any trailing horizontal space
        exec -draft "<a-:><a-;><a-k>(?=[^\n]*\h)(?=[^\n]*[^\h])[^\n]{%opt{autowrap_column},}[^\n]<ret>%opt{autowrap_column}l<a-i>w<a-;>;i<ret><esc>kxXs\h+$<ret>d "
    }
}

## Add a hook that will wrap the entire line every time a key is pressed
def autowrap-enable %{
    hook -group autowrap window InsertChar [^\n] %{
        try %{
            exec -draft "x:autowrap-selection<ret>"
        }
    }
}

## Disable the automatic line wrapping, autowrap-selection remains available though
def autowrap-disable %{
    rmhooks window autowrap
}
