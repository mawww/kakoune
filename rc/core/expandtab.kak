define-command -docstring "noexpandtab: use tab character to indent and align" \
noexpandtab %{
    remove-hooks window tabmode
    hook -group noexpandtab window NormalKey <gt> %{ try %{
        execute-keys -draft "<a-x>s^\h+<ret><a-@>"
    }}
    set-option window aligntab true
}

define-command -docstring "expandtab: use space character to indent and align" \
expandtab %{
    remove-hooks window tabmode
    hook -group tabmode window InsertChar '\t' %{ execute-keys -draft h@ }
    hook -group tabmode window InsertDelete ' ' %{ try %{
        execute-keys -draft <a-h><a-k> "^\h+.\z" <ret>I<space><esc><lt>
    }}
    set-option window aligntab false
}

define-command -docstring "smarttab: use tab character for indentation and space character for alignment" \
smarttab %{
    remove-hooks window tabmode
    hook -group tabmode window InsertKey <tab> %{ try %{
        execute-keys -draft <a-h><a-k> "^\h*.\z" <ret>
    } catch %{
        execute-keys -draft h@
    }}
    hook -group tabmode window NormalKey <gt> %{ try %{
        execute-keys -draft "<a-x>s^\h+<ret><a-@>"
    }}
    set-option window aligntab false
}

