# http://doc.rs/ron
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ron) %{
    set-option buffer filetype ron
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ron %{
    require-module ron

    hook window ModeChange pop:insert:.* -group ron-trim-indent ron-trim-indent
    hook window InsertChar \n -group ron-indent ron-indent-on-new-line
    hook window InsertChar \{ -group ron-indent ron-indent-on-opening-curly-brace
    hook window InsertChar [)}\]] -group ron-indent ron-indent-on-closing

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window ron-.+ }
}

hook -group ron-highlight global WinSetOption filetype=ron %{
    add-highlighter window/ron ref ron
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ron }
}

provide-module ron %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ron regions
add-highlighter shared/ron/code default-region group
add-highlighter shared/ron/string region %{(?<!')"} (?<!\\)(\\\\)*" fill string
add-highlighter shared/ron/raw_string region -match-capture %{(?<!')r(#*)"} %{"(#*)} fill string

add-highlighter shared/ron/line_comment1 region // $ group
add-highlighter shared/ron/line_comment1/comment fill comment
add-highlighter shared/ron/line_comment1/todo regex (TODO|NOTE|FIXME): 1:meta
add-highlighter shared/ron/line_comment2 region //[!/]{2} $ fill comment

add-highlighter shared/ron/block_comment1   region -recurse /\* /\* \*/ group
add-highlighter shared/ron/block_comment1/comment fill comment
add-highlighter shared/ron/block_comment1/todo regex (TODO|NOTE|FIXME): 1:meta

add-highlighter shared/ron/code/values regex \b(?:self|true|false|[0-9][_0-9]*(?:\.[0-9][_0-9]*|(?:\.[0-9][_0-9]*)?E[\+\-][_0-9]+)(?:f(?:32|64))?|(?:0x[_0-9a-fA-F]+|0o[_0-7]+|0b[_01]+|[0-9][_0-9]*)(?:(?:i|u|f)(?:8|16|32|64|128|size))?)\b 0:value

add-highlighter shared/ron/code/enum_variant regex \b(Some|None|Ok|Err)\b 0:value

add-highlighter shared/ron/code/ regex ^(\s*[^,:\[\]\{\}\s]+): 1:variable
add-highlighter shared/ron/code/ regex ,(\s*[^,:\[\]\{\}\s]+): 1:variable
add-highlighter shared/ron/code/ regex \{(\s*[^,:\[\]\{\}\s]+): 1:variable
add-highlighter shared/ron/code/ regex \((\s*[^,:\[\]\{\}\s]+): 1:variable

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden ron-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden ron-indent-on-new-line %~
    evaluate-commands -draft -itersel %@
        try %{
            try %[ # line comment
                evaluate-commands -draft -save-regs '/"' %[
                    # copy the commenting prefix
                    execute-keys -save-regs '' k x s ^\h*//[!/]{0,2}\h* <ret> y
                    try %[
                        # if the previous comment isn't empty, create a new one
                        execute-keys x<a-K>^\h*//[!/]{0,2}$<ret> jxs^\h*<ret>P
                    ] catch %[
                        # TODO figure out a way to not delete empty comment in current line
                        # if there is no space and text in the previous comment, remove it completely
                        execute-keys s //.*<ret> d
                    ]
                ]
            ] catch %[ # block comment
                # if the previous line isn't within a comment scope, break
                execute-keys -draft kx <a-k>^(\h*/\*|\h+\*(?!/))<ret>

                # find comment opening, validate it was not closed, and check its using star prefixes
                execute-keys -draft <a-?>/\*<ret><a-H> <a-K>\*/<ret> <a-k>\A\h*/\*([^\n]*\n\h*\*)*[^\n]*\n\h*.\z<ret>

                try %[
                    # if the previous line is opening the comment, insert star preceeded by space
                    execute-keys -draft kx<a-k>^\h*/\*<ret>
                    execute-keys -draft i*<space><esc>
                ] catch %[
                    try %[
                        # if the next line is a comment line insert a star
                        execute-keys -draft jx<a-k>^\h+\*<ret>
                        execute-keys -draft i*<space><esc>
                    ] catch %[
                        try %[
                            # if the previous line is an empty comment line, close the comment scope
                            execute-keys -draft kx<a-k>^\h+\*\h+$<ret> x1s\*(\h*)<ret>c/<esc>
                        ] catch %[
                            # if the previous line is a non-empty comment line, add a star
                            execute-keys -draft i*<space><esc>
                        ]
                    ]
                ]

                # trim trailing whitespace on the previous line
                try %[ execute-keys -draft s\h+$<ret> d ]
                # align the new star with the previous one
                execute-keys Kx1s^[^*]*(\*)<ret>&
            ]
        } catch %`
            # re-indent previous line if it starts with where to match previous block
            # string literal parsing within extern does not handle escape
            try %% execute-keys -draft k x <a-k> ^\h*where\b <ret> hh <a-?> ^\h*\b(impl|((|pub\ |pub\((crate|self|super|in\ (::)?([a-zA-Z][a-zA-Z0-9_]*|_[a-zA-Z0-9_]+)(::[a-zA-Z][a-zA-Z0-9_]*|_[a-zA-Z0-9_]+)*)\)\ )((async\ |const\ )?(unsafe\ )?(extern\ ("[^"]*"\ )?)?fn|struct|enum|union)))\b <ret> <a-S> 1<a-&> %
            # preserve previous line indent
            try %{ execute-keys -draft <semicolon> K <a-&> }
            # indent after lines ending with [{([].+ and move first parameter to own line
            try %< execute-keys -draft [c[({[],[)}\]] <ret> <a-k> \A[({[][^\n]+\n[^\n]*\n?\z <ret> L i<ret><esc> <gt> <a-S> <a-&> >
            # indent after non-empty lines not starting with operator and not ending with , or ; or {
            # XXX simplify this into a single <a-k> without s
            try %< execute-keys -draft k x s [^\h].+ <ret> <a-K> \A[-+*/&|^})<gt><lt>#] <ret> <a-K> [,<semicolon>{](\h*/[/*].*|)$ <ret> j <a-gt> >
            # indent after lines ending with {
            try %+ execute-keys -draft k x <a-k> \{$ <ret> j <a-gt> +
            # dedent after lines starting with . and ending with } or ) or , or ; or .await (} or ) or .await maybe with ?)
            try %_ execute-keys -draft k x <a-k> ^\h*\. <ret> <a-k> ([,<semicolon>]|(([})]|\.await)\?*))\h*$ <ret> j <a-lt> _
            # dedent after lines ending with " => {}" - part of empty match
            try %# execute-keys -draft k x <a-k> \ =>\ \{\}\h*$ <ret> j <a-lt> #
            # align to opening curly brace or paren when newline is inserted before a single closing
            try %< execute-keys -draft <a-h> <a-k> ^\h*[)}] <ret> h m <a-S> 1<a-&> >
            # todo dedent additional unmatched parenthesis
            # try %& execute-keys -draft k x s \((?:[^)(]+|\((?:[^)(]+|\([^)(]*\))*\))*\) l Gl s\) %sh{
                # count previous selections length
                # printf "j $(echo $kak_selections_length | wc -w) <a-lt>"
            # } &
        `
        # filter previous line
        try %{ execute-keys -draft k : ron-trim-indent <ret> }
    @
~

define-command -hidden ron-indent-on-opening-curly-brace %[
    evaluate-commands -draft -itersel %~
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ execute-keys -draft h <a-F> ) M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
        # dedent standalone { after impl and related block without any { in between
        try %@ execute-keys -draft hh <a-?> ^\h*\b(impl|((|pub\ |pub\((crate|self|super|in\ (::)?([a-zA-Z][a-zA-Z0-9_]*|_[a-zA-Z0-9_]+)(::[a-zA-Z][a-zA-Z0-9_]*|_[a-zA-Z0-9_]+)*)\)\ )((async\ |const\ )?(unsafe\ )?(extern\ ("[^"]*"\ )?)?fn|struct|enum|union))|if|for)\b <ret> <a-K> \{ <ret> <a-semicolon> <semicolon> ll x <a-k> ^\h*\{$ <ret> <a-lt> @
    ~
]

define-command -hidden ron-indent-on-closing %~
    evaluate-commands -draft -itersel %_
        # align to opening curly brace or paren when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h*[)}\]]$ <ret> h m <a-S> 1<a-&> >
    _
~
§
