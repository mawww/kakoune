# http://lua.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-lua %{
    set buffer filetype lua
}

hook global BufCreate .*[.](lua) %{
    set buffer mimetype text/x-lua
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code lua \
    string  '"' (?<!\\)(\\\\)*"       '' \
    string  "'" (?<!\\)(\\\\)*'       '' \
    comment '--' '$'                  '' \
    comment '\Q--[[' ']]'             '' \

addhl -group /lua/string fill string

addhl -group /lua/comment fill comment

addhl -group /lua/code regex \b(and|break|do|else|elseif|end|false|for|function|goto|if|in|local|nil|not|or|repeat|return|then|true|until|while)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def lua-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ %sh{
    case $kak_buffile in
        *spec/*_spec.lua)
            altfile=$(eval printf %s\\n $(printf %s\\n $kak_buffile | sed s+spec/+'*'/+';'s/_spec//))
            [ ! -f $altfile ] && echo "echo -color Error 'implementation file not found'" && exit
        ;;
        *.lua)
            path=$kak_buffile
            dirs=$(while [ $path ]; do printf %s\\n $path; path=${path%/*}; done | tail -n +2)
            for dir in $dirs; do
                altdir=$dir/spec
                if [ -d $altdir ]; then
                    altfile=$altdir/$(realpath $kak_buffile --relative-to $dir | sed s+[^/]'*'/++';'s/.lua$/_spec.lua/)
                    break
                fi
            done
            [ ! -d $altdir ] && echo "echo -color Error 'spec/ not found'" && exit
        ;;
        *)
            echo "echo -color Error 'alternative file not found'" && exit
        ;;
    esac
    printf %s\\n "edit $altfile"
}}

def -hidden _lua_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h + $ <ret> d }
    }
}

def -hidden _lua_indent_on_char %{
    eval -draft -itersel %{
        # align end structure to start
        try %{ exec -draft <a-x> <a-k> ^ \h * end $ <ret> <a-\;> <a-?> ^ \h * (for|function|if|while) <ret> s \A | \Z <ret> \' <a-&> }
        # align _else_ statements to previous _if_
        try %{ exec -draft <a-x> <a-k> ^ \h * else (if) ? $ <ret> <a-\;> <a-?> ^ \h * if <ret> s \A | \Z <ret> \' <a-&> }
    }
}

def -hidden _lua_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft K <a-&> }
        # filter previous line
        try %{ exec -draft k : _lua_filter_around_selections <ret> }
        # copy -- comment prefix and following white spaces
        try %{ exec -draft k x s ^ \h * \K -- \h * <ret> y j p }
        # indent after start structure
        try %{ exec -draft k x <a-k> ^ \h * (else|elseif|for|function|if|while) \b <ret> j <a-gt> }
        # wisely add end structure
        eval -save-regs x %{
            try %{ exec -draft k x s ^ \h + <ret> \" x y } catch %{ reg x '' }
            try %{ exec -draft k x <a-k> ^ <c-r> x (for|function|if|while) <ret> j <a-a> i X <a-\;> K <a-K> ^ <c-r> x (for|function|if|while) . * \n <c-r> x end $ <ret> j x y p j a end <esc> <a-lt> }
        }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group lua-highlight global WinSetOption filetype=lua %{ addhl ref lua }

hook global WinSetOption filetype=lua %{
    hook window InsertEnd  .* -group lua-hooks  _lua_filter_around_selections
    hook window InsertChar .* -group lua-indent _lua_indent_on_char
    hook window InsertChar \n -group lua-indent _lua_indent_on_new_line

    alias window alt lua-alternative-file
}

hook -group lua-highlight global WinSetOption filetype=(?!lua).* %{ rmhl lua }

hook global WinSetOption filetype=(?!lua).* %{
    rmhooks window lua-indent
    rmhooks window lua-hooks

    unalias window alt lua-alternative-file
}
