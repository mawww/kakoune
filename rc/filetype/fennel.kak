# http://fennel-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.]fnl %{
    set-option buffer filetype fennel
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook global WinSetOption filetype=fennel %{
    require-module fennel
    fennel-configure-window
}

hook -group fennel-highlight global WinSetOption filetype=fennel %{
    add-highlighter window/fennel ref fennel
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/fennel }
}

provide-module fennel %{

require-module lisp

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/fennel regions
add-highlighter shared/fennel/code default-region group
add-highlighter shared/fennel/comment region '(?<!\\)(?:\\\\)*\K;' '$' fill comment
add-highlighter shared/fennel/shebang region '(?<!\\)(?:\\\\)*\K#!' '$' fill comment
add-highlighter shared/fennel/string region '(?<!\\)(?:\\\\)*\K"' '(?<!\\)(?:\\\\)*"' fill string
add-highlighter shared/fennel/colon-string region '\W\K:[-\w?\\^_!$%&*+./@|<=>]' '(?![-\w?\\^_!$%&*+./@|<=>])' fill keyword
add-highlighter shared/fennel/code/ regex \\(?:space|tab|newline|return|backspace|formfeed|u[0-9a-fA-F]{4}|o[0-3]?[0-7]{1,2}|.)\b 0:string

evaluate-commands %sh{
    # Grammar
    keywords="require-macros eval-compiler doc lua hashfn macro macros import-macros pick-args pick-values macroexpand macrodebug
              do values if when each for fn lambda λ partial while set global var local let tset set-forcibly! doto match or and
              not not= collect icollect accumulate rshift lshift bor band bnot bxor with-open"
    re_keywords='\\$ \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9 \\$\\.\\.\\.'
    builtins="_G _VERSION arg assert bit32 collectgarbage coroutine debug
              dofile error getfenv getmetatable io ipairs length load
              loadfile loadstring math next os package pairs pcall
              print rawequal rawget rawlen rawset require select setfenv
              setmetatable string table tonumber tostring type unpack xpcall"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

# Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list fennel_static_words $(join "${keywords} ${builtins} false nil true" ' ')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/fennel/code/keywords regex \b($(join "${keywords} ${re_keywords}" '|'))\b 0:keyword
        add-highlighter shared/fennel/code/builtins regex \b($(join "${builtins}" '|'))\b 0:builtin
    "
}

add-highlighter shared/fennel/code/operator regex (\.|\?\.|\+|\.\.|\^|-|\*|%|/|>|<|>=|<=|=|\.\.\.|:|->|->>|-\?>|-\?>>) 0:operator
add-highlighter shared/fennel/code/value regex \b(false|nil|true|[0-9]+(:?\.[0-9])?(:?[eE]-?[0-9]+)?|0x[0-9a-fA-F])\b 0:value
add-highlighter shared/fennel/code/function_declaration regex \((?:fn|lambda|λ)\s+([\S]+) 1:function
add-highlighter shared/fennel/code/method_call regex (?:\w+|\$[0-9]{0,1}):(\S+)\b 1:function

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden fennel-configure-window %{
    set-option window static_words %opt{fennel_static_words}

    hook window ModeChange pop:insert:.* -group fennel-trim-indent fennel-trim-indent
    hook window InsertChar \n -group fennel-indent fennel-indent-on-new-line

    set-option buffer extra_word_chars '_' . / * ? + - < > ! : "'"
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window fennel-.+ }
}

define-command -hidden fennel-trim-indent lisp-trim-indent

declare-option \
    -docstring 'regex matching the head of forms which have options *and* indented bodies' \
    regex fennel_special_indent_forms \
    '(?:if|when|each|for|fn|lambda|λ|partial|while|local|var|doto|let)'

define-command -hidden fennel-indent-on-new-line %{
    # registers: i = best align point so far; w = start of first word of form
    evaluate-commands -draft -save-regs '/"|^@iw' -itersel %{
        execute-keys -draft 'gk"iZ'
        try %{
            execute-keys -draft '[bl"i<a-Z><gt>"wZ'

            try %{
                # If a special form, indent another (indentwidth - 1) spaces
                execute-keys -draft '"wze<a-K>[\s()\[\]\{\}]<ret><a-k>\A' %opt{fennel_special_indent_forms} '\z<ret>'
                execute-keys -draft '"wze<a-L>s.{' %sh{printf $(( kak_opt_indentwidth - 1 ))} '}\K.*<ret><a-;>;"i<a-Z><gt>'
            } catch %{
                # If not special and parameter appears on line 1, indent to parameter
                execute-keys -draft '"wz<a-K>[()[\]{}]<ret>e<a-K>[\s()\[\]\{\}]<ret><a-l>s\h\K[^\s].*<ret><a-;>;"i<a-Z><gt>'
            }
        }
        try %{ execute-keys -draft '[rl"i<a-Z><gt>' }
        try %{ execute-keys -draft '[Bl"i<a-Z><gt>' }
        execute-keys -draft ';"i<a-z>a&,'
    }
}

}
