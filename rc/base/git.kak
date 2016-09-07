hook global BufCreate .*COMMIT_EDITMSG %{
    set buffer filetype git-commit
}

hook global WinSetOption filetype=git-commit %{
    addhl group git-commit-highlight
    addhl -group git-commit-highlight regex "\`[^\n]{1,50}" 0:yellow
    addhl -group git-commit-highlight regex "\`[^\n]*\n\h*(?!#)([^\n]*)\n?" 1:default,red
    addhl -group git-commit-highlight regex "^\h*#[^\n]*\n" 0:cyan,default
    addhl -group git-commit-highlight regex "\b(?:(modified)|(deleted)|(new file)|(renamed)):([^\n]*)\n" 1:yellow 2:red 3:green 4:blue 5:magenta
    addhl -group git-commit-highlight ref diff # highlight potential diffs from the -v option
}

hook global WinSetOption filetype=(?!git-commit).* %{
    rmhl git-commit-highlight
}

hook global BufCreate .*git-rebase-todo %{
    set buffer filetype git-rebase
}

hook global WinSetOption filetype=git-rebase %{
    addhl group git-rebase-highlight
    addhl -group git-rebase-highlight regex "#[^\n]*\n" 0:cyan,default
    addhl -group git-rebase-highlight regex "^(pick|edit|reword|squash|fixup|exec|[persfx]) (\w+)" 1:green 2:magenta
}

hook global WinSetOption filetype=(?!git-rebase).* %{
    rmhl git-rebase-highlight
}
