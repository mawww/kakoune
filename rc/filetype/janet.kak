# http://janet-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](janet|jdn) %{
    set-option buffer filetype janet
}


hook global WinSetOption filetype=janet %{
    require-module janet

    hook window ModeChange pop:insert:.* -group janet-trim-indent janet-trim-indent
    hook window InsertChar \n -group janet-indent janet-indent-on-new-line
    set-option buffer extra_word_chars ! @ $ '%' ^ & * - _ + = : < > . ?

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window janet-.+ }
}

hook -group janet-highlight global WinSetOption filetype=janet %{
    add-highlighter window/janet ref janet
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/janet }
}

provide-module janet %{

require-module lisp

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/janet regions
add-highlighter shared/janet/code default-region group
add-highlighter shared/janet/comment region '(?<!\\)(?:\\\\)*\K#' '$' fill comment
add-highlighter shared/janet/comment-form region -recurse \( '(?<!\\)(?:\\\\)*\K\(comment ' '\)' fill comment
add-highlighter shared/janet/string  region '(^\s+```)|(?<!\\)(?:\\\\)*\K"' '(^\s+```)|(?<!\\)(?:\\\\)*"' fill string
add-highlighter shared/janet/code/ regex \b(nil|true|false)\b 0:value
add-highlighter shared/janet/code/keyword regex \W\K:[!@$%\^&*\-_+=:<>.?\w]+ 0:value
add-highlighter shared/janet/code/number regex \W\K(?:[\-+]?\dx?[\der._+a-f]*)\b 0:value
add-highlighter shared/janet/code/function-definition regex \((?:defn|fn)\s([!@$%\^&*\-_+=:<>.?\w]+) 1:function
add-highlighter shared/janet/code/function-call regex \(([!@$%\^&*\-_+=:<>.?\w/]+) 1:function
add-highlighter shared/janet/code/special regex \((def|defn|var|fn|do|quote|if|splice|while|break|set|quasiquote|unquote|upscope)\b\s 1:keyword
add-highlighter shared/janet/code/ regex \W\K(&|&opt)\W 1:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden janet-trim-indent lisp-trim-indent

declare-option \
    -docstring 'regex matching the head of forms which have options *and* indented bodies' \
    regex janet_special_indent_forms \
    '(?:def.*|while|for|fn\*?|if(-.*|)|let.*|loop|seq|with(-.*|)|when(-.*|))|defer|do|match|var'

define-command -hidden janet-indent-on-new-line %{
    # registers: i = best align point so far; w = start of first word of form
    evaluate-commands -draft -save-regs '/"|^@iw' -itersel %{
        execute-keys -draft 'gk"iZ'
        try %{
            execute-keys -draft '[bl"i<a-Z><gt>"wZ'

            try %{
                # If a special form, indent another j
                execute-keys -draft '"wze<a-k>\A' %opt{janet_special_indent_forms} '\z<ret><a-L>s.\K.*<ret><a-;>;"i<a-Z><gt>'
            } catch %{
                # If not special and parameter appears on line 1, indent to parameter
                execute-keys -draft '"wze<a-l>s\h\K[^\s].*<ret><a-;>;"i<a-Z><gt>'
            }
        }
        try %{ execute-keys -draft '[rl"i<a-Z><gt>' }
        try %{ execute-keys -draft '[Bl"i<a-Z><gt>' }
        execute-keys -draft '"i<a-z>a&,'
    }
}

}
