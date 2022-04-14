# http://lua.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](lua|rockspec) %{
    set-option buffer filetype lua
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=lua %{
    require-module lua

    hook window ModeChange pop:insert:.* -group lua-trim-indent lua-trim-indent
    hook window InsertChar .* -group lua-indent lua-indent-on-char
    hook window InsertChar \n -group lua-indent lua-indent-on-new-line
    hook window InsertChar \n -group lua-insert lua-insert-on-new-line

    alias window alt lua-alternative-file

    hook -once -always window WinSetOption filetype=.* %{
        remove-hooks window lua-.+
        unalias window alt lua-alternative-file
    }
}

hook -group lua-highlight global WinSetOption filetype=lua %{
    add-highlighter window/lua ref lua
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/lua }
}


provide-module lua %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/lua regions
add-highlighter shared/lua/code default-region group
add-highlighter shared/lua/raw_string  region -match-capture   '\[(=*)\[' '\](=*)\]' fill string
add-highlighter shared/lua/raw_comment region -match-capture '--\[(=*)\[' '\](=*)\]' fill comment
add-highlighter shared/lua/double_string region '"'   (?<!\\)(?:\\\\)*" fill string
add-highlighter shared/lua/single_string region "'"   (?<!\\)(?:\\\\)*' fill string
add-highlighter shared/lua/comment       region '--'  $                 fill comment

add-highlighter shared/lua/code/function_declaration regex \b(?:function\h+)(?:\w+\h*\.\h*)*([a-zA-Z_]\w*)\( 1:function
add-highlighter shared/lua/code/function_call regex \b([a-zA-Z_]\w*)\h*(?=[\(\{]) 1:function
add-highlighter shared/lua/code/keyword regex \b(break|do|else|elseif|end|for|function|goto|if|in|local|repeat|return|then|until|while)\b 0:keyword
add-highlighter shared/lua/code/value regex \b(false|nil|true|[0-9]+(:?\.[0-9])?(:?[eE]-?[0-9]+)?|0x[0-9a-fA-F])\b 0:value
add-highlighter shared/lua/code/symbolic_operator regex (\+|-|\*|/|%|\^|==?|~=|<=?|>=?|\.\.|\.\.\.|#) 0:operator
add-highlighter shared/lua/code/keyword_operator regex \b(and|or|not)\b 0:operator
add-highlighter shared/lua/code/module regex \b(_G|_ENV)\b 0:module
add-highlighter shared/lua/code/attribute regex \B(<[a-zA-Z_]\w*>)\B 0:attribute

# Commands
# ‾‾‾‾‾‾‾‾

define-command lua-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ evaluate-commands %sh{
    case $kak_buffile in
        *spec/*_spec.lua)
            altfile=$(eval printf %s\\n $(printf %s\\n $kak_buffile | sed s+spec/+'*'/+';'s/_spec//))
            [ ! -f $altfile ] && echo "fail 'implementation file not found'" && exit
        ;;
        *.lua)
            altfile=""
            altdir=""
            path=$kak_buffile
            dirs=$(while [ $path ]; do printf %s\\n $path; path=${path%/*}; done | tail -n +2)
            for dir in $dirs; do
                altdir=$dir/spec
                if [ -d $altdir ]; then
                    altfile=$altdir/$(realpath $kak_buffile --relative-to $dir | sed s+[^/]'*'/++';'s/.lua$/_spec.lua/)
                    break
                fi
            done
            [ ! -d "$altdir" ] && echo "fail 'spec/ not found'" && exit
        ;;
        *)
            echo "fail 'alternative file not found'" && exit
        ;;
    esac
    printf %s\\n "edit $altfile"
}}

define-command -hidden lua-trim-indent %[
    # remove trailing whitespaces
    try %[ execute-keys -draft -itersel x s \h+$ <ret> d ]
]

define-command -hidden lua-indent-on-char %[
    evaluate-commands -no-hooks -draft -itersel %[
        # unindent middle and end structures
        try %[ execute-keys -draft \
            <a-h><a-k>^\h*(\b(end|else|elseif|until)\b|[)}])$<ret> \
            :lua-indent-on-new-line<ret> \
            <a-lt>
        ]
    ]
]

define-command -hidden lua-indent-on-new-line %[
    evaluate-commands -no-hooks -draft -itersel %[
        # remove trailing white spaces from previous line
        try %[ execute-keys -draft k : lua-trim-indent <ret> ]
        # preserve previous non-empty line indent
        try %[ execute-keys -draft ,gh<a-?>^[^\n]+$<ret>s\A|.\z<ret>)<a-&> ]
        # add one indentation level if the previous line is not a comment and:
        #     - starts with a block keyword that is not closed on the same line,
        #     - or contains an unclosed function expression,
        #     - or ends with an enclosed '(' or '{'
        try %[ execute-keys -draft \
            , Kx \
            <a-K>\A\h*--<ret> \
            <a-K>\A[^\n]*\b(end|until)\b<ret> \
            <a-k>\A(\h*\b(do|else|elseif|for|function|if|repeat|while)\b|[^\n]*[({]$|[^\n]*\bfunction\b\h*[(])<ret> \
            <a-:><semicolon><a-gt>
        ]
    ]
]

define-command -hidden lua-insert-on-new-line %[
    evaluate-commands -no-hooks -draft -itersel %[
        # copy -- comment prefix and following white spaces
        try %[ execute-keys -draft kxs^\h*\K--\h*<ret> y gh j x<semicolon> P ]
        # wisely add end structure
        evaluate-commands -save-regs x %[
            # save previous line indent in register x
            try %[ execute-keys -draft kxs^\h+<ret>"xy ] catch %[ reg x '' ]
            try %[
                # check that starts with a block keyword that is not closed on the same line
                execute-keys -draft \
                    kx \
                    <a-k>^\h*\b(else|elseif|do|for|function|if|while)\b|[^\n]\bfunction\b\h*[(]<ret> \
                    <a-K>\bend\b<ret>
                # check that the block is empty and is not closed on a different line
                execute-keys -draft <a-a>i <a-K>^[^\n]+\n[^\n]+\n<ret> jx <a-K>^<c-r>x\b(else|elseif|end)\b<ret>
                # auto insert end
                execute-keys -draft o<c-r>xend<esc>
                # auto insert ) for anonymous function
                execute-keys -draft kx<a-k>\([^)\n]*function\b<ret>jjA)<esc>
            ]
        ]
    ]
]

§
