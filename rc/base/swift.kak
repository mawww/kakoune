hook global BufCreate .*\.(swift) %{
    set buffer filetype swift
}

add-highlighter -group / regions -default code swift \
    string %{(?<!')"} %{(?<!\\)(\\\\)*"} '' \
    comment /\* \*/ '' \
    comment // $ ''

add-highlighter -group /swift/string fill string
add-highlighter -group /swift/comment fill comment

add-highlighter -group /swift/comment regex "\<(TODO|XXX|MARK)\>" 0:red

add-highlighter -group /swift/code regex %{\<(true|false|nil)\>|\<-?(?!\$)\d+[fdiu]?|'((\\.)?|[^'\\])'} 0:value
add-highlighter -group /swift/code regex "\<(let|var|while|in|for|if|else|do|switch|case|default|break|continue|return|try|catch|throw|new|delete|and|or|not|operator|explicit|func|import|return|init|deinit|get|set)\>" 0:keyword
add-highlighter -group /swift/code regex "\<as\>[!?]?" 0:keyword
add-highlighter -group /swift/code regex "(\$[0-9])\>" 0:keyword
add-highlighter -group /swift/code regex "\<(const|mutable|auto|namespace|inline|static|volatile|class|struct|enum|union|public|protected|private|typedef|virtual|friend|extern|typename|override|final|required|convenience|dynamic)\>" 0:attribute

add-highlighter -group /swift/code regex "\<(self|nil|id|super)\>" 0:value
add-highlighter -group /swift/code regex "\<(Bool|String|UInt|UInt16|UInt32|UInt64|UInt8)\>" 0:type
add-highlighter -group /swift/code regex "\<(IBAction|IBOutlet)\>" 0:attribute
add-highlighter -group /swift/code regex "@\w+\>" 0:attribute

hook -group swift-highlight global WinSetOption filetype=swift %{ add-highlighter ref swift }
hook -group swift-highlight global WinSetOption filetype=(?!swift).* %{ remove-highlighter swift }
