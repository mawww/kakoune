hook global BufCreate .*COMMIT_EDITMSG %{
    set buffer filetype git-commit
}

hook -group git-commit-highlight global WinSetOption filetype=git-commit %{
    add-highlighter group git-commit-highlight
    add-highlighter -group git-commit-highlight regex "\`[^\n]{1,50}" 0:yellow
    add-highlighter -group git-commit-highlight regex "\`[^\n]*\n\h*(?!#)([^\n]*)\n?" 1:default,red
    add-highlighter -group git-commit-highlight regex "^\h*#[^\n]*\n" 0:cyan,default
    add-highlighter -group git-commit-highlight regex "\b(?:(modified)|(deleted)|(new file)|(renamed)):([^\n]*)\n" 1:yellow 2:red 3:green 4:blue 5:magenta
    add-highlighter -group git-commit-highlight ref diff # highlight potential diffs from the -v option
}

hook -group git-commit-highlight global WinSetOption filetype=(?!git-commit).* %{ remove-highlighter git-commit-highlight }

hook global BufCreate .*git-rebase-todo %{
    set buffer filetype git-rebase
}

hook -group git-rebase-highlight global WinSetOption filetype=git-rebase %{
    add-highlighter group git-rebase-highlight
    add-highlighter -group git-rebase-highlight regex "#[^\n]*\n" 0:cyan,default
    add-highlighter -group git-rebase-highlight regex "^(pick|edit|reword|squash|fixup|exec|[persfx]) (\w+)" 1:green 2:magenta
}

hook -group git-rebase-highlight global WinSetOption filetype=(?!git-rebase).* %{ remove-highlighter git-rebase-highlight }
