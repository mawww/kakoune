hook global BufCreate .*git-revise-todo %{
  set-option buffer filetype git-revise
}

hook global WinSetOption filetype=git-revise %{
  require-module "git-revise"
}

hook -group git-revise-highlight global WinSetOption filetype=git-revise %{
  add-highlighter window/git-revise ref git-revise
  hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-revise }
}

provide-module git-revise %{
  add-highlighter shared/git-revise group
  add-highlighter shared/git-revise/ regex "^\h*#[^\n]*\n" 0:comment
  add-highlighter shared/git-revise/ regex "^(?:(pick|index|[pi])|(reword|squash|fixup|cut|[rsfc])) (\w+)" 1:keyword 2:value 3:meta
}
