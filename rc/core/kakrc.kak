# http://kakoune.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    set buffer filetype kak
}

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code kakrc \
    comment (^|\h)\K\# $ '' \
    double_string %{(^|\h)"} %{(?<!\\)(\\\\)*"} '' \
    single_string %{(^|\h)'} %{(?<!\\)(\\\\)*'} '' \
    shell '%sh\{' '\}' '\{'

%sh{
    # Grammar
    keywords="hook|remove-hooks|rmhooks|add-highlighter|addhl|remove-highlighter|rmhl|exec|eval|source|runtime|define-command|def|alias"
    keywords="${keywords}|unalias|declare-option|decl|echo|edit|set-option|set|unset-option|unset|map|unmap|set-face|face|prompt|menu|info"
    keywords="${keywords}|try|catch|rename-client|rename-buffer|rename-session|change-directory|colorscheme"
    values="default|black|red|green|yellow|blue|magenta|cyan|white"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=kak %{
        set window static_words '${keywords}:${values}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /kakrc/code regex \b(${keywords})\b 0:keyword
        addhl -group /kakrc/code regex \b(${values})\b 0:value
    "
}

addhl -group /kakrc/code regex \brgb:[0-9a-fA-F]{6}\b 0:value
addhl -group /kakrc/code regex (?:\bhook)\h+(?:-group\h+\S+\h+)?(?:(global|buffer|window)|(\S+))\h+(\S+) 1:attribute 2:Error 3:identifier
addhl -group /kakrc/code regex (?:\bset)\h+(?:-add)?\h+(?:(global|buffer|window)|(\S+))\h+(\S+) 1:attribute 2:Error 3:identifier
addhl -group /kakrc/code regex (?:\bmap)\h+(?:(global|buffer|window)|(\S+))\h+(?:(normal|insert|menu|prompt|goto|view|user|object)|(\S+))\h+(\S+) 1:attribute 2:Error 3:attribute 4:Error 5:identifier

addhl -group /kakrc/double_string fill string
addhl -group /kakrc/single_string fill string
addhl -group /kakrc/comment fill comment
addhl -group /kakrc/shell ref sh

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden kak-indent-on-new-line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ exec -draft k <a-x> s \h+$ <ret> d }
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*#\h* <ret> y jgh P }
        # indent after line ending with %[[:punct:]]
        try %{ exec -draft k <a-x> <a-k> \%[[:punct:]]$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group kak-highlight global WinSetOption filetype=kak %{ addhl ref kakrc }

hook global WinSetOption filetype=kak %{
    hook window InsertChar \n -group kak-indent kak-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window InsertEnd .* -group kak-indent %{ try %{ exec -draft \; <a-x> s ^\h+$ <ret> d } }
}

hook -group kak-highlight global WinSetOption filetype=(?!kak).* %{ rmhl kakrc }
hook global WinSetOption filetype=(?!kak).* %{ rmhooks window kak-indent }
