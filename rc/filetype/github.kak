hook global BufCreate .*/CODEOWNERS %{
    set-option buffer filetype codeowners
}

hook global WinSetOption filetype=codeowners %{
    require-module codeowners
}

hook -group codeowners-hightlight global WinSetOption filetype=codeowners %{
    add-highlighter window/codeowners ref codeowners
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/codeowners }
}

provide-module codeowners %{
add-highlighter shared/codeowners regions
add-highlighter shared/codeowners/comments region ^# $ group
add-highlighter shared/codeowners/comments/ fill comment
}
