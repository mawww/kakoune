hook global BufCreate .*\.(diff|patch) %{
    set-option buffer filetype diff
}

add-highlighter shared/diff group
add-highlighter shared/diff/ regex "^\+[^\n]*\n" 0:green,default
add-highlighter shared/diff/ regex "^-[^\n]*\n" 0:red,default
add-highlighter shared/diff/ regex "^@@[^\n]*@@" 0:cyan,default

hook -group diff-highlight global WinSetOption filetype=diff %{
    add-highlighter window/diff ref diff
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/diff }
}

define-command \
    -docstring %{diff-select-file: Select surrounding patch file} \
    -params 0 \
    diff-select-file %{
    evaluate-commands -itersel -save-regs 'ose/' %{
        try %{
            execute-keys '"oZgl<a-?>^diff <ret>;"sZ' 'Ge"eZ'
            try %{ execute-keys '"sz?\n(?=diff )<ret>"e<a-Z><lt>' }
            execute-keys '"ez'
        } catch %{
            execute-keys '"oz'
            fail 'Not in a diff file'
        }
    }
}

define-command \
    -docstring %{diff-select-hunk: Select surrounding patch hunk} \
    -params 0 \
    diff-select-hunk %{
    evaluate-commands -itersel -save-regs 'ose/' %{
        try %{
            execute-keys '"oZgl<a-?>^@@ <ret>;"sZ' 'Ge"eZ'
            try %{ execute-keys '"sz?\n(?=diff )<ret>"e<a-Z><lt>' }
            try %{ execute-keys '"sz?\n(?=@@ )<ret>"e<a-Z><lt>' }
            execute-keys '"ez'
        } catch %{
            execute-keys '"oz'
            fail 'Not in a diff hunk'
        }
    }
}
