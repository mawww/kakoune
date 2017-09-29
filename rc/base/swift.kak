hook global BufCreate .*\.(swift) %{
    set buffer filetype swift
}

add-highlighter -group / regions -default code swift \
    string %{(?<!')"} %{(?<!\\)(\\\\)*"} '' \
    comment /\* \*/ '' \
    comment // $ ''

add-highlighter -group /swift/string fill string
add-highlighter -group /swift/comment fill comment

add-highlighter -group /swift/comment regex "\b(TODO|XXX|MARK)\b" 0:red

add-highlighter -group /swift/code regex %{\b(true|false|nil)\b|\b-?(?!\$)\d+[fdiu]?|'((\\.)?|[^'\\])'} 0:value
add-highlighter -group /swift/code regex "\b(let|var|while|in|for|if|else|do|switch|case|default|break|continue|return|try|catch|throw|new|delete|and|or|not|operator|explicit|func|import|return|init|deinit|get|set)\b" 0:keyword
add-highlighter -group /swift/code regex "\bas\b[!?]?" 0:keyword
add-highlighter -group /swift/code regex "(\$[0-9])\b" 0:keyword
add-highlighter -group /swift/code regex "\b(const|mutable|auto|namespace|inline|static|volatile|class|struct|enum|union|public|protected|private|typedef|virtual|friend|extern|typename|override|final|required|convenience|dynamic)\b" 0:attribute

add-highlighter -group /swift/code regex "\b(self|nil|id|super)\b" 0:value
add-highlighter -group /swift/code regex "\b(Bool|String|UInt|UInt16|UInt32|UInt64|UInt8)\b" 0:type
add-highlighter -group /swift/code regex "\b(IBAction|IBOutlet)\b" 0:attribute
add-highlighter -group /swift/code regex "@\w+\b" 0:attribute

hook -group swift-highlight global WinSetOption filetype=swift %{ add-highlighter ref swift }
hook -group swift-highlight global WinSetOption filetype=(?!swift).* %{ remove-highlighter swift }
