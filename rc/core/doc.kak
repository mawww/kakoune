def -params 1..2 \
    -shell-candidates %{
        echo "
            commands
            execeval
            expansions
            faces
            faq
            highlighters
            hooks
            keys
            mapping
            options
            registers
            scopes
        " | sed 's/[[:space:]]//g'
    } \
    doc -docstring %{doc <topic> [<keyword>]: open a buffer containing documentation about a given topic
An optional keyword argument can be passed to the command, which will be automatically selected in the documentation} \
    %{ %sh{
    printf 'man %s %s\n' "kak-${1}" "${2}"
} }

alias global help doc
