
# requires rc/base/javascript.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ts)x? %{
    set buffer filetype typescript
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / group typescript
add-highlighter -group /typescript ref javascript

add-highlighter -group /typescript regex \b(array|boolean|date|number|object|regexp|string|symbol)\b 0:type

# Keywords grabbed from https://github.com/Microsoft/TypeScript/issues/2536
add-highlighter -group /typescript regex \b(enum|as|implements|interface|package|private|protected|public|static|constructor|declare|get|module|set|type|from|of)\b 0:keyword

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
