hook global BufCreate .*(COMMIT_EDITMSG|MERGE_MSG) %{
    set-option buffer filetype git-commit
}

hook global BufCreate .*/NOTES_EDITMSG %{
    set-option buffer filetype git-notes
}

hook global BufCreate .*(\.gitconfig|git/config) %{
    set-option buffer filetype ini
}

hook global BufCreate .*git-rebase-todo %{
    set-option buffer filetype git-rebase
}

hook -once global BufSetOption filetype=git-commit %{
    require-module git-commit
}
hook -once global BufSetOption filetype=git-notes %{
    require-module git-notes
}
hook -once global BufSetOption filetype=git-rebase %{
    require-module git-rebase
}

provide-module git-commit %{
hook -group git-commit-highlight global WinSetOption filetype=git-commit %{
    add-highlighter window/git-commit-highlight regions
    add-highlighter window/git-commit-highlight/diff region '^diff --git' '^(?=diff --git)' ref diff # highlight potential diffs from the -v option
    add-highlighter window/git-commit-highlight/comments region '^\h*#' '$' group
    add-highlighter window/git-commit-highlight/comments/ fill comment
    add-highlighter window/git-commit-highlight/comments/ regex "\b(?:(modified)|(deleted)|(new file)|(renamed|copied)):([^\n]*)$" 1:yellow 2:red 3:green 4:blue 5:magenta

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-commit-highlight }
}}

provide-module git-notes %{
hook -group git-commit-highlight global WinSetOption filetype=git-notes %{
    add-highlighter window/git-notes-highlight regex '^\h*#[^\n]*$' 0:comment

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-notes-highlight }
}}


provide-module git-rebase %{
hook -group git-rebase-highlight global WinSetOption filetype=git-rebase %{
    add-highlighter window/git-rebase-highlight group
    add-highlighter window/git-rebase-highlight/ regex "#[^\n]*\n" 0:comment
    add-highlighter window/git-rebase-highlight/ regex "^(pick|edit|reword|squash|fixup|exec|break|drop|label|reset|merge|[persfxbdltm]) (\w+)" 1:keyword 2:meta

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-rebase-highlight }
}}
