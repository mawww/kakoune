
# requires rc/base/javascript.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ts)x? %{
    set buffer filetype typescript
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code typescript \
    double_string '"'  (?<!\\)(\\\\)*"         '' \
    single_string "'"  (?<!\\)(\\\\)*'         '' \
    literal       "`"  (?<!\\)(\\\\)*`         '' \
    comment       //   '$'                     '' \
    comment       /\*  \*/                     '' \
    regex         /    (?<!\\)(\\\\)*/[gimuy]* '' \
    division '[\w\)\]](/|(\h+/\h+))' '\w' '' # Help Kakoune to better detect /…/ literals

# Regular expression flags are: g → global match, i → ignore case, m → multi-lines, u → unicode, y → sticky
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp

add-highlighter -group /typescript/double_string ref javascript/double_string
add-highlighter -group /typescript/single_string ref javascript/single_string
add-highlighter -group /typescript/regex         ref javascript/regex
add-highlighter -group /typescript/comment       ref javascript/comment
add-highlighter -group /typescript/literal       ref javascript/literal
add-highlighter -group /typescript/code          ref javascript/code

add-highlighter -group /typescript/code regex \b(array|boolean|date|number|object|regexp|string|symbol)\b 0:type

# Keywords grabbed from https://github.com/Microsoft/TypeScript/issues/2536
add-highlighter -group /typescript/code regex \b(enum|as|implements|interface|package|private|protected|public|static|constructor|declare|get|module|set|type|from|of|readonly)\b 0:keyword

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group typescript-highlight global WinSetOption filetype=typescript %{ add-highlighter ref typescript }

hook global WinSetOption filetype=javascript %{
    hook window InsertEnd  .* -group typescript-hooks  javascript-filter-around-selections
    hook window InsertChar .* -group typescript-indent javascript-indent-on-char
    hook window InsertChar \n -group typescript-indent javascript-indent-on-new-line
}

hook -group typescript-highlight global WinSetOption filetype=(?!typescript).* %{ remove-highlighter typescript }

hook global WinSetOption filetype=(?!typescript).* %{
    remove-hooks window typescript-indent
    remove-hooks window typescript-hooks
}
