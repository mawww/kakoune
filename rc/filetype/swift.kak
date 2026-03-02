#
# MARK: - detection
#

hook global BufCreate .*\.(swift) %{
    set-option buffer filetype swift
}

#
# MARK: - initialisation
#

hook global WinSetOption filetype=swift %<
    require-module swift
    hook window ModeChange pop:insert:.* -group swift-trim-indent swift-trim-indent

    hook window InsertChar \n -group swift-insert swift-insert-on-new-line
    hook window InsertChar \n -group swift-indent swift-indent-on-new-line
    hook window InsertChar \} -group swift-indent swift-indent-on-closing

    hook -once -always window WinSetOption filetype=.* %< remove-hooks window swift-.+ >
>

hook -group swift-highlight global WinSetOption filetype=swift %{
    add-highlighter window/swift ref swift
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/swift }
}

provide-module swift %§

#
# MARK: - highlighters
#

add-highlighter shared/swift regions
add-highlighter shared/swift/code default-region group

add-highlighter shared/swift/string_multiline region %{(?<!')"""} %{(?<!\\)(\\\\)*"""} regions
add-highlighter shared/swift/string_multiline/base default-region fill string
add-highlighter shared/swift/string_multiline/interpolation region -recurse \( \Q\( \) fill meta

add-highlighter shared/swift/string region %{(?<!')"} %{(?<!\\)(\\\\)*"} regions
add-highlighter shared/swift/string/base default-region fill string
add-highlighter shared/swift/string/interpolation region -recurse \( \Q\( \) fill meta

add-highlighter shared/swift/comment region /\* \*/ group
add-highlighter shared/swift/line_comment region // $ ref swift/comment

add-highlighter shared/swift/comment/ fill comment

add-highlighter shared/swift/code/ regex %{\b(true|false|nil)\b|\b-?(?!\$)\d+[fdiu]?|'((\\.)?|[^'\\])'} 0:value
add-highlighter shared/swift/code/ regex "\b(let|var|while|in|for|if|guard|else|do|switch|case|default|break|continue|return|try|catch|throw|new|delete|and|or|not|operator|explicit|func|import|return|init|deinit|get|set)\b" 0:keyword
add-highlighter shared/swift/code/ regex "\bas\b[!?]?" 0:keyword
add-highlighter shared/swift/code/ regex "(\$[0-9])\b" 0:keyword
add-highlighter shared/swift/code/ regex "\b(const|mutable|auto|namespace|inline|static|volatile|class|struct|enum|union|extension|open|public|protected|private|fileprivate|internal|typedef|virtual|friend|extern|typename|override|final|required|convenience|dynamic)\b" 0:attribute

add-highlighter shared/swift/code/ regex "\b(self|nil|id|super)\b" 0:value
add-highlighter shared/swift/code/ regex "\b(Bool|String|UInt|UInt16|UInt32|UInt64|UInt8)\b" 0:type
add-highlighter shared/swift/code/ regex "\b(IBAction|IBOutlet)\b" 0:attribute
add-highlighter shared/swift/code/ regex "@\w+\b" 0:attribute

#
# MARK: commands
#

define-command -hidden swift-trim-indent %{
    # delete trailing whitespace
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden swift-insert-on-new-line %<
    try %[
        evaluate-commands -draft -save-regs '/"\' %[
            # copy // comments prefix and following whitespace
            execute-keys -save-regs '' k x1s^\h*(//+\h*)<ret> y
            try %[
                # if the previous comment isn't empty, create a new one
                execute-keys x<a-K>^\h*//+\h*$<ret> jxs^\h*<ret>P
            ] catch %[
                # if there is no text in the previous comment, remove it completely
                execute-keys d
            ]
        ]

        # trim trailing whitespace on the previous line
        try %[ execute-keys -draft k x s\h+$<ret> d ]
    ]
>

define-command -hidden swift-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve indent level
        try %< execute-keys -draft <semicolon> K <a-&> >
        try %<
            # only if we didn't copy a comment
            execute-keys -draft x <a-K> ^\h*// <ret>
            # indent after lines ending in {
            try %< execute-keys -draft k x <a-k> \{\h*$ <ret> j <a-gt> >
            # indent after lines ending in 'in' (closure parameters)
            try %< execute-keys -draft k x <a-k> \bin\h*$ <ret> j <a-gt> >
            # deindent closing } when after cursor
            try %< execute-keys -draft x <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> >
        >
        # filter previous line
        try %< execute-keys -draft k : swift-trim-indent <ret> >
    >
>

define-command -hidden swift-indent-on-closing %<
    # align lone } to indent level of opening line
    try %< execute-keys -draft -itersel <a-h> <a-k> ^\h*\}$ <ret> h m <a-S> 1<a-&> >
>

§
