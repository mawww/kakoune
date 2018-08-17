hook global BufCreate .*(COMMIT_EDITMSG|MERGE_MSG) %{
    set-option buffer filetype git-commit
}

hook global BufCreate .*(\.gitconfig|git/config) %{
    set-option buffer filetype ini
}

set-face global diffadd green
set-face global diffremove red
set-face global diffmeta cyan

hook -group git-commit-highlight global WinSetOption filetype=git-commit %{
    add-highlighter window/git-commit-highlight group
    add-highlighter window/git-commit-highlight/ regex "^\h*#[^\n]*\n" 0:comment
    add-highlighter window/git-commit-highlight/ regex "\b(?:(modified)|(deleted)|(new file)|(renamed|copied)):([^\n]*)\n" 1:keyword 2:diffremove 3:diffadd 4:diffmeta 5:meta
    add-highlighter window/git-commit-highlight/ ref diff # highlight potential diffs from the -v option
}

hook -group git-commit-highlight global WinSetOption filetype=(?!git-commit).* %{ remove-highlighter window/git-commit-highlight }

hook global BufCreate .*git-rebase-todo %{
    set-option buffer filetype git-rebase
}

hook -group git-rebase-highlight global WinSetOption filetype=git-rebase %{
    add-highlighter window/git-rebase-highlight group
    add-highlighter window/git-rebase-highlight/ regex "#[^\n]*\n" 0:comment
    add-highlighter window/git-rebase-highlight/ regex "^(pick|edit|reword|squash|fixup|exec|delete|[persfx]) (\w+)" 1:keyword 2:meta
}

hook -group git-rebase-highlight global WinSetOption filetype=(?!git-rebase).* %{ remove-highlighter window/git-rebase-highlight }
