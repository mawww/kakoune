
# requires rc/base/javascript.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ts)x? %{
    set-option buffer filetype typescript
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code typescript \
    double_string '"'  (?<!\\)(\\\\)*"         '' \
    single_string "'"  (?<!\\)(\\\\)*'         '' \
    literal       "`"  (?<!\\)(\\\\)*`         '' \
    comment       //   '$'                     '' \
    comment       /\*  \*/                     '' \
    regex         /    (?<!\\)(\\\\)*/[gimuy]* '' \
    division '[\w\)\]](/|(\h+/\h+))' '\w' '' # Help Kakoune to better detect /…/ literals

# Regular expression flags are: g → global match, i → ignore case, m → multi-lines, u → unicode, y → sticky
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp

add-highlighter shared/typescript/double_string ref javascript/double_string
add-highlighter shared/typescript/single_string ref javascript/single_string
add-highlighter shared/typescript/regex         ref javascript/regex
add-highlighter shared/typescript/comment       ref javascript/comment
add-highlighter shared/typescript/literal       ref javascript/literal
add-highlighter shared/typescript/code          ref javascript/code

add-highlighter shared/typescript/code regex \b(array|boolean|date|number|object|regexp|string|symbol)\b 0:type

# Keywords grabbed from https://github.com/Microsoft/TypeScript/issues/2536
add-highlighter shared/typescript/code regex \b(enum|as|implements|interface|package|private|protected|public|static|constructor|declare|get|module|set|type|from|of|readonly)\b 0:keyword

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group typescript-highlight global WinSetOption filetype=typescript %{ add-highlighter window ref typescript }

hook global WinSetOption filetype=javascript %{
    hook window InsertEnd  .* -group typescript-hooks  javascript-filter-around-selections
    hook window InsertChar .* -group typescript-indent javascript-indent-on-char
    hook window InsertChar \n -group typescript-indent javascript-indent-on-new-line
}

hook -group typescript-highlight global WinSetOption filetype=(?!typescript).* %{ remove-highlighter window/typescript }

hook global WinSetOption filetype=(?!typescript).* %{
    remove-hooks window typescript-indent
    remove-hooks window typescript-hooks
}
