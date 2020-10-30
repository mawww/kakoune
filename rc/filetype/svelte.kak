hook global WinCreate .*\.svelte %[
    set-option window filetype svelte
]

hook global WinSetOption filetype=(svelte) %{
    require-module html

    hook window ModeChange pop:insert:.* -group "%val{hook_param_capture_1}-trim-indent"  html-trim-indent
    hook window InsertChar '>' -group "%val{hook_param_capture_1}-indent" html-indent-on-greater-than
    hook window InsertChar \n -group "%val{hook_param_capture_1}-indent" html-indent-on-new-line

    hook -once -always window WinSetOption "filetype=.*" "
        remove-hooks window ""%val{hook_param_capture_1}-.+""
    "
}

hook -group svelte-highlight global WinSetOption filetype=(svelte) %{
    add-highlighter "window/%val{hook_param_capture_1}" ref html
    hook -once -always window WinSetOption "filetype=.*" "
        remove-highlighter ""window/%val{hook_param_capture_1}""
    "
}
