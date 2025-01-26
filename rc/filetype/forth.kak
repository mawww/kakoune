hook global BufCreate .+\.(fth|4th|fs|forth) %{
    set-option buffer filetype forth
}

hook global WinSetOption filetype=forth %{
    require-module forth
}

hook -group forth-highlight global WinSetOption filetype=forth %{
    add-highlighter window/forth ref forth
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/forth }
}

provide-module forth %{

add-highlighter shared/forth regions
add-highlighter shared/forth/comment region '\(' '\)' fill comment

}
