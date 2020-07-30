# zig syntax highlighting for kakoune (https://ziglang.org)
#
# based off of https://github.com/ziglang/zig.vim/blob/master/syntax/zig.vim
# as well as https://ziglang.org/documentation/master/#Grammar

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.]zig %{
  set-option buffer filetype zig
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=zig %<
    require-module zig
    hook window ModeChange pop:insert:.* -group zig-trim-indent zig-trim-indent
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

add-highlighter shared/zig/doc_comment      region '///[^/]' '$'       fill documentation
add-highlighter shared/zig/comment          region '//' '$'            fill comment

# TODO: highlight escape sequences within strings
add-highlighter shared/zig/string_double    region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/zig/string_single    region "'" (?<!\\)(\\\\)*' fill string
add-highlighter shared/zig/string_multiline region '\\\\' '$'          fill string

# attributes
add-highlighter shared/zig/code/ regex '\b(?:const|var|extern|packed|export|pub|noalias|inline|noinline|comptime|callconv|volatile|allowzero|align|linksection|threadlocal)\b' 0:attribute
# structures
add-highlighter shared/zig/code/ regex '\b(?:struct|enum|union|error)\b' 0:attribute

# statement
add-highlighter shared/zig/code/ regex '\b(?:break|return|continue|asm|defer|errdefer|unreachable|try|catch|async|noasync|await|suspend|resume)\b' 0:keyword
# conditional
add-highlighter shared/zig/code/ regex '\b(?:if|else|switch|and|or|orelse)\b' 0:keyword
# repeat
add-highlighter shared/zig/code/ regex '\b(?:while|for)\b' 0:keyword
# other keywords
add-highlighter shared/zig/code/ regex '\b(?:fn|usingnamespace|test)\b' 0:keyword

# types
add-highlighter shared/zig/code/ regex '\b(?:bool|f16|f32|f64|f128|void|noreturn|type|anyerror|anyframe)\b' 0:type
add-highlighter shared/zig/code/ regex '\b(?:i0|u0|isize||usize|comptime_int|comptime_float)\b' 0:type
add-highlighter shared/zig/code/ regex '\b(?:[iu][1-9]\d*)\b' 0:type
add-highlighter shared/zig/code/ regex '\b(?:c_short|c_ushort|c_int|c_uint|c_long|c_ulong|c_longlong|c_ulonglong|c_longdouble|c_void)\b' 0:type

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
add-highlighter shared/zig/code/ regex "@(?:addWithOverflow|ArgType|atomicLoad|atomicStore|bitCast|breakpoint)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:alignCast|alignOf|cDefine|cImport|cInclude)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:cUndef|canImplicitCast|clz|cmpxchgWeak|cmpxchgStrong|compileError)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:compileLog|ctz|popCount|divExact|divFloor|divTrunc)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:embedFile|export|tagName|TagType|errorName|call)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:errorReturnTrace|fence|fieldParentPtr|field|unionInit)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:frameAddress|import|newStackCall|asyncCall|intToPtr|IntType)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:memberCount|memberName|memberType|as)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:memcpy|memset|mod|mulWithOverflow|splat)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:bitOffsetOf|byteOffsetOf|OpaqueType|panic|ptrCast)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:ptrToInt|rem|returnAddress|setCold|Type|shuffle)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:setRuntimeSafety|setEvalBranchQuota|setFloatMode)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:setGlobalLinkage|setGlobalSection|shlExact|This|hasDecl|hasField)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:shlWithOverflow|shrExact|sizeOf|bitSizeOf|sqrt|byteSwap|subWithOverflow|intCast|floatCast|intToFloat|floatToInt|boolToInt|errSetCast)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:truncate|typeId|typeInfo|typeName|TypeOf|atomicRmw|bytesToSlice|sliceToBytes)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:intToError|errorToInt|intToEnum|enumToInt|setAlignStack|frame|Frame|frameSize|bitReverse|Vector)\b" 0:function
add-highlighter shared/zig/code/ regex "@(?:sin|cos|exp|exp2|log|log2|log10|fabs|floor|ceil|trunc|round)\b" 0:function

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden zig-trim-indent %{
    # delete trailing whitespace
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden zig-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        try %<
            # copy // or /// comments prefix and following whitespace
            execute-keys -draft k <a-x> s ^\h*\K///?\h* <ret> y gh j P
            # preserve indent level
            try %< execute-keys -draft <semicolon> K <a-&> >
        > catch %<
            # preserve indent level
            try %< execute-keys -draft <semicolon> K <a-&> >
            # indent after lines ending in {
            try %< execute-keys -draft k <a-x> <a-k> \{\h*$ <ret> j <a-gt> >
            # deindent closing } when after cursor
            try %< execute-keys -draft <a-x> <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> >
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
