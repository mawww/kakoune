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
    add-highlighter "window/svelte" ref svelte
    hook -once -always window WinSetOption "filetype=.*" "
        remove-highlighter ""window/svelte""
    "
}

add-highlighter shared/svelte regions
add-highlighter shared/svelte/comment region <!--     -->                  fill comment
add-highlighter shared/svelte/tag     region <          >                  regions
add-highlighter shared/svelte/style   region <style\b.*?>\K  (?=</style>)  ref css
add-highlighter shared/svelte/script  region <script\b.*?>\K (?=</script>) ref javascript

add-highlighter shared/svelte/block region \{((#|:|/)\w+)? \} regions
add-highlighter shared/svelte/block/ default-region fill meta
add-highlighter shared/svelte/block/inner region -recurse \{ \{((#|:|/)\w+)?\K (?=\}) ref javascript

add-highlighter shared/svelte/tag/base default-region ref html/tag
add-highlighter shared/svelte/tag/block region -recurse \{ \{ \} ref svelte/block
