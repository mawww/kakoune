hook global WinCreate .*\.svelte %[
    set-option window filetype svelte
]

hook global WinSetOption filetype=(svelte) %{
    require-module html

    hook window ModeChange pop:insert:.* -group "svelte-trim-indent"  html-trim-indent
    hook window InsertChar '>' -group "svelte-indent" html-indent-on-greater-than
    hook window InsertChar \n -group "svelte-indent" html-indent-on-new-line

    hook -once -always window WinSetOption "filetype=.*" "
        remove-hooks window ""svelte-.+""
    "
}

hook -group svelte-highlight global WinSetOption filetype=(svelte) %{
    add-highlighter "window/svelte" ref html
    hook -once -always window WinSetOption "filetype=.*" "
        remove-highlighter ""window/svelte""
    "
}
