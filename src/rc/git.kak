hook global BufCreate .*COMMIT_EDITMSG %{
    setb filetype git-commit
}

hook global WinSetOption filetype=git-commit %{
    addhl group git-commit-highlight
    addhl -group git-commit-highlight regex "#[^\n]*\n" 0:cyan,default
    addhl -group git-commit-highlight regex "\<(modified|deleted|new file):[^\n]*\n" 0:magenta,default
    addhl -group git-commit-highlight regex "\<(modified|deleted|new file):" 0:red,default
}

hook global WinSetOption filetype=(?!git-commit).* %{
    rmhl git-commit-highlight
}

hook global BufCreate .*git-rebase-todo %{
    setb filetype git-rebase
}

hook global WinSetOption filetype=git-rebase %{
    addhl group git-rebase-highlight
    addhl -group git-rebase-highlight regex "#[^\n]*\n" 0:cyan,default
    addhl -group git-rebase-highlight regex "^(pick|edit|reword|squash|fixup|exec|[persfx]) \w+" 0:magenta,default
    addhl -group git-rebase-highlight regex "^(pick|edit|reword|squash|fixup|exec|[persfx])" 0:green,default
}

hook global WinSetOption filetype=(?!git-rebase).* %{
    rmhl git-rebase-highlight
}
