# http://common-lisp.net
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](lisp) %{
    set-option buffer filetype lisp
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=lisp %{
    require-module lisp

    hook window ModeChange pop:insert:.* -group lisp-trim-indent lisp-trim-indent
    hook window InsertChar \n -group lisp-indent lisp-indent-on-new-line
    set-option buffer extra_word_chars '_' '+' '-' '*' '/' '@' '$' '%' '^' '&' '_' '=' '<' '>' '~' '.'

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window lisp-.+ }
}

hook -group lisp-highlight global WinSetOption filetype=lisp %{
    add-highlighter window/lisp ref lisp
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/lisp }
}

provide-module lisp %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/lisp regions
add-highlighter shared/lisp/code default-region group
add-highlighter shared/lisp/string  region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/lisp/comment region ';' '$'             fill comment

add-highlighter shared/lisp/code/ regex \b(nil|true|false)\b 0:value
add-highlighter shared/lisp/code/ regex (((\Q***\E)|(///)|(\Q+++\E)){1,3})|(1[+-])|(<|>|<=|=|>=) 0:operator
add-highlighter shared/lisp/code/ regex \b(def[a-z]+|if|do|let|lambda|catch|and|assert|while|def|do|fn|finally|let|loop|new|quote|recur|set!|throw|try|var|case|if-let|if-not|when|when-first|when-let|when-not|(cond(->|->>)?))\b 0:keyword
add-highlighter shared/lisp/code/ regex (#?(['`:]|,@?))?\b[a-zA-Z][\w!$%&*+./:<=>?@^_~-]* 0:variable
add-highlighter shared/lisp/code/ regex \*[a-zA-Z][\w!$%&*+./:<=>?@^_~-]*\* 0:variable
add-highlighter shared/lisp/code/ regex (\b\d+)?\.\d+([eEsSfFdDlL]\d+)?\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden lisp-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

declare-option \
    -docstring 'regex matching the head of forms which have options *and* indented bodies' \
    regex lisp_special_indent_forms \
    '(?:def.*|if(-.*|)|let.*|lambda|with-.*|when(-.*|))'

define-command -hidden lisp-indent-on-new-line %{
    # registers: i = best align point so far; w = start of first word of form
    evaluate-commands -draft -save-regs '/"|^@iw' -itersel %{
        execute-keys -draft 'gk"iZ'
        try %{
            execute-keys -draft '[bl"i<a-Z><gt>"wZ'

            try %{
                # If a special form, indent another (indentwidth - 1) spaces
                execute-keys -draft '"wze<a-k>\A' %opt{lisp_special_indent_forms} '\z<ret>'
                execute-keys -draft '"wze<a-L>s.{' %sh{printf $(( kak_opt_indentwidth - 1 ))} '}\K.*<ret><a-;>;"i<a-Z><gt>'
            } catch %{
                # If not "special" form and parameter appears on line 1, indent to parameter
                execute-keys -draft '"wz<a-K>[()\[\]{}]<ret>e<a-l>s\h\K[^\s].*<ret><a-;>;"i<a-Z><gt>'
            }
        }
        try %{ execute-keys -draft '[rl"i<a-Z><gt>' }
        try %{ execute-keys -draft '[Bl"i<a-Z><gt>' }
        execute-keys -draft ';"i<a-z>a&,'
    }
}

}
