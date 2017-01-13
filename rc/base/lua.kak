# http://lua.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](lua) %{
    set buffer filetype lua
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code lua \
    string  '"' (?<!\\)(\\\\)*"       '' \
    string  "'" (?<!\\)(\\\\)*'       '' \
    comment '--' '$'                  '' \
    comment '\Q--[[' ']]'             '' \

add-highlighter -group /lua/string fill string

add-highlighter -group /lua/comment fill comment

add-highlighter -group /lua/code regex \b(and|break|do|else|elseif|end|false|for|function|goto|if|in|local|nil|not|or|repeat|return|then|true|until|while)\b 0:keyword

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

def -hidden lua-filter-around-selections %{
    eval -no-hooks -draft -itersel %{
        # remove trailing white spaces
        try %{ exec -draft <a-x>s\h+$<ret>d }
    }
}

def -hidden lua-indent-on-char %{
    eval -no-hooks -draft -itersel %{
        # align middle and end structures to start and indent when necessary, elseif is already covered by else
        try %{ exec -draft <a-x><a-k>^\h*(else)$<ret><a-\;><a-?>^\h*(if)<ret>s\A|\Z<ret>'<a-&> }
        try %{ exec -draft <a-x><a-k>^\h*(end)$<ret><a-\;><a-?>^\h*(for|function|if|while)<ret>s\A|\Z<ret>'<a-&> }
    }
}

def -hidden lua-indent-on-new-line %{
    eval -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space>K<a-&> }
        # filter previous line
        try %{ exec -draft k:lua-filter-around-selections<ret> }
        # indent after start structure
        try %{ exec -draft k<a-x><a-k>^\h*(else|elseif|for|function|if|while)\b<ret>j<a-gt> }
    }
}

def -hidden lua-insert-on-new-line %{
    eval -no-hooks -draft -itersel %{
        # copy -- comment prefix and following white spaces
        try %{ exec -draft k<a-x>s^\h*\K--\h*<ret>yjp }
        # wisely add end structure
        eval -save-regs x %{
            try %{ exec -draft k<a-x>s^\h+<ret>"xy } catch %{ reg x '' }
            try %{ exec -draft k<a-x><a-k>^<c-r>x(for|function|if|while)<ret>j<a-a>iX<a-\;>K<a-K>^<c-r>x(for|function|if|while).*\n<c-r>x(else|end|elseif[^\n]*)$<ret>jxypjaend<esc><a-lt> }
        }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group lua-highlight global WinSetOption filetype=lua %{ add-highlighter ref lua }

hook global WinSetOption filetype=lua %{
    hook window InsertChar .* -group lua-indent lua-indent-on-char
    hook window InsertChar \n -group lua-insert lua-insert-on-new-line
    hook window InsertChar \n -group lua-indent lua-indent-on-new-line

    alias window alt lua-alternative-file
}

hook -group lua-highlight global WinSetOption filetype=(?!lua).* %{ remove-highlighter lua }

hook global WinSetOption filetype=(?!lua).* %{
    remove-hooks window lua-indent
    remove-hooks window lua-insert

    unalias window alt lua-alternative-file
}
