# Detection
hook global BufCreate .*\.fidl %{
    set-option buffer filetype fidl
}

hook global WinSetOption filetype=fidl %<
    require-module fidl
    hook window ModeChange pop:insert:.* -group fidl-trim-indent fidl-trim-indent
    hook window InsertChar \n -group fidl-indent fidl-indent-on-new-line
    hook window InsertChar [)}] -group fidl-indent fidl-indent-on-closing
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window fidl-.+ }
>

hook -group fidl-highlight global WinSetOption filetype=fidl %{
    add-highlighter window/fidl ref fidl
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/fidl }
}

provide-module fidl %§

# Highlighters

add-highlighter shared/fidl regions
add-highlighter shared/fidl/code default-region group

add-highlighter shared/fidl/string region \" (?<!\\)(\\\\)*" fill string

add-highlighter shared/fidl/line_doc region ///(?!/) $ fill documentation
add-highlighter shared/fidl/line_comment region // $ group
add-highlighter shared/fidl/line_comment/comment fill comment
add-highlighter shared/fidl/line_comment/todo regex TODO|FIXME: 0:meta

add-highlighter shared/fidl/attributes region @[a-zA-Z] \b fill meta

add-highlighter shared/fidl/code/keywords regex \b(as|bits|compose|const|enum|error|flexible|optional|library|protocol|resource|service|strict|struct|table|type|union|using)\b 0:keyword
add-highlighter shared/fidl/code/types regex \b(array|bool|bytes?|client_end|float(32|64)|server_end|string|u?int(8|16|32|64)|vector)\b 0:type

add-highlighter shared/fidl/code/literals group
add-highlighter shared/fidl/code/literals/bool regex \b(true|false)\b 0:value
add-highlighter shared/fidl/code/literals/decimal regex \b([0-9]+(\.[0-9]+)?)\b 0:value
add-highlighter shared/fidl/code/literals/hexadecimal regex \b(0x[0-9a-fA-F])\b 0:value
add-highlighter shared/fidl/code/literals/binary regex \b(0b[01])\b 0:value

# Commands

define-command -hidden fidl-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden fidl-indent-on-new-line %~
    evaluate-commands -draft -itersel %@
        try %{ # line comment
            # copy the commenting prefix
            execute-keys -draft k x s ^\h*/{2,}\h* <ret> yjghP
        } catch %`
            # preserve previous line indent
            try %{ execute-keys -draft <semicolon> K <a-&> }
            # align to opening ( or { if possible
            try %+
                execute-keys -draft <a-k> [)}] <ret> m <a-S> 1<a-&>
            + catch %+
                # indent after lines ending with ( or {
                try %! execute-keys -draft k x <a-k> [({]$ <ret> j <a-gt> !
            +
        `
        # remove trailing white spaces
        try %{ execute-keys -draft k : fidl-trim-indent <ret> }
    @
~

define-command -hidden fidl-indent-on-closing %~
    evaluate-commands -draft -itersel %@
        # align to opening ( or { when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h*[)}]$ <ret> m <a-S> 1<a-&> >
    @
~

§
