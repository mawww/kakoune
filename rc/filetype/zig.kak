# zig syntax highlighting for kakoune (https://ziglang.org)
#
# based off of https://github.com/ziglang/zig.vim/blob/master/syntax/zig.vim
# as well as https://ziglang.org/documentation/master/#Grammar

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](zig|zon) %{
  set-option buffer filetype zig
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=zig %<
    require-module zig
    hook window ModeChange pop:insert:.* -group zig-trim-indent zig-trim-indent
    hook window InsertChar \n -group zig-insert zig-insert-on-new-line
    hook window InsertChar \n -group zig-indent zig-indent-on-new-line
    hook window InsertChar \} -group zig-indent zig-indent-on-closing

    hook -once -always window WinSetOption filetype=.* %< remove-hooks window zig-.+ >
>

hook -group zig-highlight global WinSetOption filetype=zig %{
    require-module zig
    add-highlighter window/zig ref zig
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/zig }
}

provide-module zig %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/zig regions
add-highlighter shared/zig/code default-region group

add-highlighter shared/zig/doc_comment region '///(?=[^/])' '$' fill documentation
add-highlighter shared/zig/comment region '//' '$' fill comment

# strings and characters
add-highlighter shared/zig/string region '"' (?<!\\)(\\\\)*" group
add-highlighter shared/zig/string/ fill string
add-highlighter shared/zig/string/ regex '(?:\\n|\\r|\\t|\\\\|\\''|\\"|\\x[0-9a-fA-F]{2}|\\u\{[0-9a-fA-F]+\})' 0:meta

add-highlighter shared/zig/character region "'" (?<!\\)(\\\\)*' group
add-highlighter shared/zig/character/ fill string
add-highlighter shared/zig/character/ regex '(?:\\n|\\r|\\t|\\\\|\\''|\\"|\\x[0-9a-fA-F]{2}|\\u\{[0-9a-fA-F]+\})' 0:meta

add-highlighter shared/zig/multiline_string region '\\\\' '$' fill string

# attributes
add-highlighter shared/zig/code/ regex '\b(?:const|var|extern|packed|export|pub|noalias|inline|noinline|comptime|callconv|volatile|allowzero|addrspace|align|linksection|threadlocal)\b' 0:attribute
# structures
add-highlighter shared/zig/code/ regex '\b(?:struct|enum|union|error|opaque)\b' 0:attribute

# statement
add-highlighter shared/zig/code/ regex '\b(?:break|return|continue|asm|defer|errdefer|unreachable|try|catch|suspend|nosuspend|resume)\b' 0:keyword
# conditional
add-highlighter shared/zig/code/ regex '\b(?:if|else|switch|and|or|orelse)\b' 0:keyword
# repeat
add-highlighter shared/zig/code/ regex '\b(?:while|for)\b' 0:keyword
# other keywords
add-highlighter shared/zig/code/ regex '\b(?:fn|test)\b' 0:keyword

# types
add-highlighter shared/zig/code/ regex '\b(?:bool|f16|f32|f64|f80|f128|void|noreturn|type|anyerror|anyframe|anytype|anyopaque)\b' 0:type
add-highlighter shared/zig/code/ regex '\b(?:i0|u0|isize|usize|comptime_int|comptime_float)\b' 0:type
add-highlighter shared/zig/code/ regex '\b(?:[iu][1-9]\d*)\b' 0:type
add-highlighter shared/zig/code/ regex '\b(?:c_char|c_short|c_ushort|c_int|c_uint|c_long|c_ulong|c_longlong|c_ulonglong|c_longdouble)\b' 0:type

# primitive values
add-highlighter shared/zig/code/ regex '\b(?:true|false|null|undefined)\b' 0:value

# integer literals
add-highlighter shared/zig/code/ regex '\b[0-9](_?[0-9])*\b' 0:value
add-highlighter shared/zig/code/ regex '\b0x[0-9a-fA-F](_?[0-9a-fA-F])*\b' 0:value
add-highlighter shared/zig/code/ regex '\b0o[0-7](_?[0-7])*\b' 0:value
add-highlighter shared/zig/code/ regex '\b0b[01](_?[01])*\b' 0:value

# float literals
add-highlighter shared/zig/code/ regex '\b[0-9]+\.[0-9]+(?:[eE][-+]?[0-9]+)?\b' 0:value
add-highlighter shared/zig/code/ regex '\b0x[0-9a-fA-F]+\.[0-9a-fA-F]+(?:[pP][-+]?[0-9a-fA-F]+)?\b' 0:value
add-highlighter shared/zig/code/ regex '\b[0-9]+\.?[eE][-+]?[0-9]+\b' 0:value
add-highlighter shared/zig/code/ regex '\b0x[0-9a-fA-F]+\.?[eE][-+]?[0-9a-fA-F]+\b' 0:value

# operators
add-highlighter shared/zig/code/ regex '(?:\+|-|\*|/|%|=|<|>|&|\||\^|~|\?|!)' 0:operator

# builtin functions
add-highlighter shared/zig/code/ regex "@(?:addrSpaceCast|addWithOverflow|alignCast|alignOf|as|atomicLoad|atomicRmw|atomicStore|bitCast|bitOffsetOf|bitSizeOf|branchHint|breakpoint|mulAdd|byteSwap|bitReverse|offsetOf|call|cDefine|cImport|cInclude|clz|cmpxchgStrong|cmpxchgWeak|compileError|compileLog|constCast|ctz|cUndef|cVaArg|cVaCopy|cVaEnd|cVaStart|disableInstrumentation|disableIntrinsics|divExact|divFloor|divTrunc|embedFile|enumFromInt|errorFromInt|errorName|errorReturnTrace|errorCast|export|extern|field|fieldParentPtr|FieldType|floatCast|floatFromInt|frameAddress|hasDecl|hasField|import|inComptime|intCast|intFromBool|intFromEnum|intFromError|intFromFloat|intFromPtr|max|memcpy|memmove|memset|min|wasmMemorySize|wasmMemoryGrow|mod|mulWithOverflow|panic|popCount|prefetch|ptrCast|ptrFromInt|rem|returnAddress|select|setEvalBranchQuota|setFloatMode|setRuntimeSafety|shlExact|shlWithOverflow|shrExact|shuffle|sizeOf|splat|reduce|src|sqrt|sin|cos|tan|exp|exp2|log|log2|log10|abs|floor|ceil|trunc|round|subWithOverflow|tagName|This|trap|truncate|Type|typeInfo|typeName|TypeOf|unionInit|Vector|volatileCast|workGroupId|workGroupSize|workItemId)\b" 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden zig-trim-indent %{
    # delete trailing whitespace
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden zig-insert-on-new-line %<
    try %[
        evaluate-commands -draft -save-regs '/"' %[
            # copy // or /// comments prefix or \\ string literal prefix and following whitespace
            execute-keys -save-regs '' k x1s^\h*((///?|\\\\)+\h*)<ret> y
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

define-command -hidden zig-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve indent level
        try %< execute-keys -draft <semicolon> K <a-&> >
        try %<
            # only if we didn't copy a comment or multiline string
            execute-keys -draft x <a-K> ^\h*(//|\\\\) <ret>
            # indent after lines ending in {
            try %< execute-keys -draft k x <a-k> \{\h*$ <ret> j <a-gt> >
            # deindent closing } when after cursor
            try %< execute-keys -draft x <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> >
        >
        # filter previous line
        try %< execute-keys -draft k : zig-trim-indent <ret> >
    >
>

define-command -hidden zig-indent-on-closing %<
    # align lone } to indent level of opening line
    try %< execute-keys -draft -itersel <a-h> <a-k> ^\h*\}$ <ret> h m <a-S> 1<a-&> >
>

§
