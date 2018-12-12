declare-option -docstring "amount of spaces that should be treated as single tab character when deleting spaces" \
int softtabstop 0

declare-option -docstring "displays current tab handling mode" \
str smarttab_mode ''

declare-option -hidden int oldindentwidth %opt{indentwidth}
define-command -hidden smarttab-set %{ evaluate-commands %sh{
    if [ $kak_opt_indentwidth -eq 0 ]; then
        echo "set-option window indentwidth $kak_opt_oldindentwidth"
    else
        echo "set-option window oldindentwidth $kak_opt_indentwidth"
    fi
}}

define-command -docstring "noexpandtab: use tab character to indent and align" \
noexpandtab %{
    set-option window smarttab_mode 'noexpandtab'
    remove-hooks window smarttab-mode
    smarttab-set
    set-option window indentwidth 0
    set-option window aligntab true
    hook -group smarttab-mode window InsertDelete ' ' %{ try %sh{
        if [ $kak_opt_softtabstop -gt 1 ]; then
            echo 'execute-keys -draft <a-h><a-k> "^\h+.\z" <ret>I<space><esc><lt>'
        fi
    } catch %{
        try %{ execute-keys -draft h %opt{softtabstop}<s-h> 2<s-l> s "\h+\z" <ret>d }
    }}
}

define-command -docstring "expandtab: use space character to indent and align" \
expandtab %{
    set-option window smarttab_mode 'expandtab'
    remove-hooks window smarttab-mode
    smarttab-set
    set-option window aligntab false
    hook -group smarttab-mode window InsertChar '\t' %{ execute-keys -draft h@ }
    hook -group smarttab-mode window InsertDelete ' ' %{ try %sh{
        if [ $kak_opt_softtabstop -gt 1 ]; then
            echo 'execute-keys -draft <a-h><a-k> "^\h+.\z" <ret>I<space><esc><lt>'
        fi
    } catch %{
        try %{ execute-keys -draft h %opt{softtabstop}<s-h> 2<s-l> s "\h+\z" <ret>d }
    }}
}

define-command -docstring "smarttab: use tab character for indentation and space character for alignment" \
smarttab %{
    set-option window smarttab_mode 'smarttab'
    remove-hooks window smarttab-mode
    smarttab-set
    set-option window indentwidth 0
    set-option window aligntab false
    hook -group smarttab-mode window InsertChar '\t' %{ try %{
        execute-keys -draft <a-h><a-k> "^\h*.\z" <ret>
    } catch %{
        execute-keys -draft h@
    }}
    hook -group smarttab-mode window InsertDelete ' ' %{ try %sh{
        if [ $kak_opt_softtabstop -gt 1 ]; then
            echo 'execute-keys -draft <a-h><a-k> "^\h+.\z" <ret>I<space><esc><lt>'
        fi
    } catch %{
        try %{ execute-keys -draft h %opt{softtabstop}<s-h> 2<s-l> s "\h+\z" <ret>d }
    }}
}

