# Nix package manager language
# https://nixos.org/nix/manual/

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](nix) %{
    set-option buffer filetype nix
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=nix %{
    require-module nix

    hook window ModeChange pop:insert:.* -group nix-trim-indent nix-trim-indent
    hook window InsertChar .* -group nix-indent nix-indent-on-char
    hook window InsertChar \n -group nix-insert nix-insert-on-new-line
    hook window InsertChar \n -group nix-indent nix-indent-on-new-line

    set-option buffer extra_word_chars _ -
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window nix-.+ }
}

hook -group nix-highlight global WinSetOption filetype=nix %{
    add-highlighter window/nix ref nix
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/nix }
}

provide-module nix %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/nix regions
add-highlighter shared/nix/code  default-region group
# Define strings. They can contain interpolated nix code,
# which itself can contain strings.
# Note that we currently cannot properly support nesting of the same-delimiter strings
# because of https://github.com/mawww/kakoune/issues/1670
add-highlighter shared/nix/double_string region '"'    (?<!\\)(?:\\\\)*"         regions
# this is hard one: it is terminated by '', but ''$, ''\* and ''' are escapes.
add-highlighter shared/nix/indent_string region "''"   (?<!')(?:''')*''(?![\\'$]) regions
add-highlighter shared/nix/comment1      region '#'    '$'                       fill comment
add-highlighter shared/nix/comment2      region /\*    \*/                       fill comment


#add-highlighter shared/nix/code/ regex "([a-zA-Z_][a-zA-Z0-9-_.]*)\s*([=])" 1:variable
add-highlighter shared/nix/code/ regex "(\b|-)[0-9]*\.?[0-9eE]+\b" 0:value

add-highlighter shared/nix/double_string/str default-region fill string
add-highlighter shared/nix/double_string/variable region -recurse \{ (?<!\\)(\\\\)*\$\{ \} ref nix
add-highlighter shared/nix/indent_string/str default-region fill string
# FIXME: the opening regex is not ideal. See https://nixos.org/nix/manual/#idm140737317975776
# It should usually match "${", should match "'''${" (as "'''" is escaped itself),
# but should not match "''${" and "''\${".
# Seems that negative lookbehind semantics is not enough for some complex cases.
add-highlighter shared/nix/indent_string/variable region -recurse \{ (?<![^']'')\$\{ \} ref nix

add-highlighter shared/nix/code/ regex \b(true|false|null|let|in|with|if|then|else)\b 0:keyword
add-highlighter shared/nix/code/ regex \b(rec)\b\s*\{ 1:keyword
# Those are builtin functions available in global scope.
# They should not be assigned to.
add-highlighter shared/nix/code/ regex '[^.]\s*\b(builtins|inherit|baseNameOf|derivation|dirOf|fetchTarball|import|isNull|map|removeAttrs|throw|toString)\b\s*[^=]' 1:builtin

add-highlighter shared/nix/code/ regex '\b\s*(\.)\s*\b'  1:operator
add-highlighter shared/nix/code/ regex '[^-a-zA-Z0-9_](-+)' 1:operator
add-highlighter shared/nix/code/ regex '\?'        0:operator
add-highlighter shared/nix/code/ regex '\+\+=?'    0:operator
add-highlighter shared/nix/code/ regex '(\*|/|\+)' 0:operator
add-highlighter shared/nix/code/ regex '!'         0:operator
add-highlighter shared/nix/code/ regex '//=?'      0:operator
add-highlighter shared/nix/code/ regex '[<>]=?\??' 0:operator
add-highlighter shared/nix/code/ regex '(==|!=)'   0:operator
add-highlighter shared/nix/code/ regex '(&&|\|\|)' 0:operator
add-highlighter shared/nix/code/ regex '->'        0:operator
add-highlighter shared/nix/code/ regex \bor\b      0:operator

# override any operators matched before
# path:
add-highlighter shared/nix/code/ regex '\s\(*(\.?\.?[-A-Za-z0-9/_+.]*/[-A-Za-z0-9/_+.]*)[;?]?' 1:meta
# imported path:
add-highlighter shared/nix/code/ regex <[-A-Za-z0-9/_+.]+> 0:meta
# RFC 2396 URIs can be used without quoting. Strangely, "string" ends URL but ''indented'' one doesn't
# List of prohibited characters was tested manually in nix-repl as it is not properly documented.
add-highlighter shared/nix/code/ regex '([^:/?#\s]+):([^#(){}\[\]";`|\s\\]+)' 0:string

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden nix-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden nix-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m s \A|.\z <ret> 1<a-&> /
    >
>

define-command -hidden nix-insert-on-new-line %<
    evaluate-commands -draft -itersel %<
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K#\h* <ret> y gh j P }
    >
>

define-command -hidden nix-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : nix-trim-indent <ret> }
        # indent after lines beginning / ending with opener token
        try %_ execute-keys -draft k x <a-k> ^\h*[[{]|[[{]$ <ret> j <a-gt> _
        # deindent closer token(s) when after cursor
        try %_ execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> _
    >
>

§
