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

add-highlighter shared/swift/comment region /\* \*/ group
add-highlighter shared/swift/line_comment region // $ ref swift/comment
add-highlighter shared/swift/comment/ fill comment

add-highlighter shared/swift/string_multiline region %{(?<!')"""} %{(?<!\\)(\\\\)*"""} regions
add-highlighter shared/swift/string_multiline/base default-region fill string
add-highlighter shared/swift/string_multiline/interpolation region -recurse \( \Q\( \) fill meta

add-highlighter shared/swift/string region %{(?<!')"} %{(?<!\\)(\\\\)*"} regions
add-highlighter shared/swift/string/base default-region fill string
add-highlighter shared/swift/string/interpolation region -recurse \( \Q\( \) fill meta

# backticked identifiers
add-highlighter shared/swift/backtick_identifier region ` ` fill meta

# discard literal
add-highlighter shared/swift/code/ regex "\b_\b" 0:comment

# values
add-highlighter shared/swift/code/ regex \b(?:true|false|nil)\b 0:value
add-highlighter shared/swift/code/ regex \b(?:self|super)\b 0:value

# Numeric literals with underscores, hex, binary, octal, floats (plus/minus are not painted, matching Xcode)
#   - Decimal with underscores: 1_000_000, 3.141_592_653_59
#   - Hexadecimal: 0xFF, 0x1A_2B, 0xDEAD_BEEF
#   - Hex floats: 0xF.8p2, +0x1_A.F_Fp3
#   - Binary: 0b1010, 0b1111_0000
#   - Octal: 0o17, +0o7_7_7, 0o7_7_7
#   - Decimal floats: 6.28, 1.5e10, +1.5e-5, 1.5E+10
#   - Negative numbers: -42, -3.14
#
# Pattern breakdown:
#   [-+]?  # optional plus or minus
#   (?:
#     0x[0-9a-fA-F][_0-9a-fA-F]*            # hex integer/float start
#        (?:\.[0-9a-fA-F][_0-9a-fA-F]*)?    # optional hex fraction
#        (?:[pP][+-]?[0-9][_0-9]*)?         # optional binary exponent
#     |0o[0-7][_0-7]*                       # octal
#     |0b[01][_01]*                         # binary
#     |[0-9][_0-9]*                         # decimal integer/float start
#        (?:\.[0-9][_0-9]*)?                # optional decimal fraction
#        (?:[eE][+-]?[0-9][_0-9]*)?         # optional decimal exponent
#   )
#
add-highlighter shared/swift/code/ regex \b[+-]?(?:0x[0-9a-fA-F][_0-9a-fA-F]*(?:\.[0-9a-fA-F][_0-9a-fA-F]*)?(?:[pP][+-]?[0-9][_0-9]*)?|0o[0-7][_0-7]*|0b[01][_01]*|[0-9][_0-9]*(?:\.[0-9][_0-9]*)?(?:[eE][+-]?[0-9][_0-9]*)?)\b 0:value

# keywords
add-highlighter shared/swift/code/ regex "\b(let|var|while|in|for|if|guard|else|do|switch|case|default|break|continue|return|try|catch|throw|operator|func|import|init|deinit|get|set|defer|repeat|fallthrough|async|await|throws|rethrows|inout|where|is|subscript|macro|protocol|typealias|actor|class|struct|enum|extension|some|any|associatedtype|distributed|isolated|nonisolated|consuming|borrowing|borrow|move|discard|sending|nonsending)\b" 0:keyword
add-highlighter shared/swift/code/ regex "\bas\b[!?]?" 0:keyword
add-highlighter shared/swift/code/ regex "(\$[0-9])\b" 0:keyword

# types
add-highlighter shared/swift/code/ regex "[\[]?\b(Bool|String|Character|Int|Int8|Int16|Int32|Int64|Int128|UInt|UInt8|UInt16|UInt32|UInt64|UInt128|Float|Float16|Float32|Float64|Float80|Double|Void|Never|Any|AnyObject|AnyClass|Optional|Array|Dictionary|Set|Range|ClosedRange|PartialRangeFrom|PartialRangeThrough|PartialRangeUpTo|Result|Error|Equatable|Hashable|Comparable|Codable|Encodable|Decodable|Sendable|CaseIterable|CodingKey|Task|TaskGroup|AsyncStream|AsyncThrowingStream|GlobalActor|UnsafePointer|UnsafeMutablePointer|UnsafeRawPointer|UnsafeMutableRawPointer|UnsafeBufferPointer|UnsafeMutableBufferPointer|UnsafeRawBufferPointer|UnsafeMutableRawBufferPointer|Unmanaged|AutoreleasingUnsafeMutablePointer|SIMD|SIMD2|SIMD3|SIMD4|SIMD8|SIMD16|SIMD32|SIMD64|SIMDMask|SIMDScalar|SIMDStorage)\b[\]]?[!?]?" 0:type

# compilation directives
add-highlighter shared/swift/code/ regex "#(if|elseif|else|endif|available|unavailable|warning|error|sourceLocation|file|fileID|filePath|line|column|function|dsohandle|selector|keyPath|colorLiteral|imageLiteral|fileLiteral)\b" 0:meta
add-highlighter shared/swift/code/ regex "\b(canImport|os|arch|swift|compiler|targetEnvironment)[\s\(]+\b" 1:meta

# attributes
add-highlighter shared/swift/code/ regex "\b(inline|static|open|public|private|fileprivate|internal|package|override|final|required|convenience|dynamic|lazy|mutating|nonmutating|indirect|weak|unowned|didSet|willSet)\b" 0:attribute
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
