hook global BufCreate .*\.(swift) %{
    set-option buffer filetype swift
}

hook global WinSetOption filetype=swift %{
    require-module swift
}

hook -group swift-highlight global WinSetOption filetype=swift %{
    add-highlighter window/swift ref swift
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/swift }
}


provide-module swift %{

add-highlighter shared/swift regions
add-highlighter shared/swift/code default-region group
add-highlighter shared/swift/string_multiline region %{(?<!')"""} %{(?<!\\)(\\\\)*"""} ref swift/string
add-highlighter shared/swift/string region %{(?<!')"} %{(?<!\\)(\\\\)*"} fill string
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

}
