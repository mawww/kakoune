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

hook global BufCreate .*git-revise-todo %{
  set-option buffer filetype git-revise
}

hook global WinSetOption filetype=git-(commit|notes|rebase|revise) %{
    require-module "git-%val{hook_param_capture_1}"
}

hook -group git-commit-highlight global WinSetOption filetype=git-commit %{
    add-highlighter window/git-commit ref git-commit
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-commit }
}

hook -group git-notes-highlight global WinSetOption filetype=git-notes %{
    add-highlighter window/git-notes ref git-notes
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-notes }
}

hook -group git-rebase-highlight global WinSetOption filetype=git-rebase %{
    add-highlighter window/git-rebase ref git-rebase
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-rebase }
}

hook -group git-revise-highlight global WinSetOption filetype=git-revise %{
  add-highlighter window/git-revise ref git-revise
  hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-revise }
}

provide-module git-commit %{
add-highlighter shared/git-commit regions
add-highlighter shared/git-commit/diff region '^diff --git' '^(?=diff --git)' ref diff # highlight potential diffs from the -v option
add-highlighter shared/git-commit/comments region ^# $ group
add-highlighter shared/git-commit/comments/ fill comment
add-highlighter shared/git-commit/comments/ regex "\b(?:(modified)|(deleted)|(new file)|(renamed|copied)):([^\n]*)$" 1:yellow 2:red 3:green 4:blue 5:magenta
}

provide-module git-notes %{
add-highlighter shared/git-notes regex ^#[^\n]*$ 0:comment
}

provide-module git-rebase %{
add-highlighter shared/git-rebase group
add-highlighter shared/git-rebase/ regex "^\h*#[^\n]*\n" 0:comment
add-highlighter shared/git-rebase/ regex "^(?:(pick|p)|(edit|reword|squash|fixup|exec|break|drop|label|reset|merge|[ersfxbdltm])) (\w+)" 1:keyword 2:value 3:meta
}

provide-module git-revise %{
add-highlighter shared/git-revise group
add-highlighter shared/git-revise/ regex "^\h*#[^\n]*\n" 0:comment
add-highlighter shared/git-revise/ regex "^(?:(pick|index|[pi])|(reword|squash|fixup|cut|[rsfc])) (\w+)" 1:keyword 2:value 3:meta
}
