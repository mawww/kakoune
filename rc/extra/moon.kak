# http://moonscript.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require commenting.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-moon %{
    set buffer filetype moon
}

hook global BufCreate .*[.](moon) %{
    set buffer mimetype text/x-moon
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code moon \
    double_string '"'  (?<!\\)(\\\\)*" '' \
    single_string "'"  (?<!\\)(\\\\)*' '' \
    comment       '--' '$'             '' \

addhl -group /moon/double_string fill string
addhl -group /moon/double_string regions regions interpolation \Q#{ \} \{
addhl -group /moon/double_string/regions/interpolation fill meta

addhl -group /moon/single_string fill string

addhl -group /moon/comment fill comment

addhl -group /moon/code regex ([.\\](?=[A-Za-z]\w*))|(\b[A-Za-z]\w*:)|(\b[A-Za-z]\w*\K!+)|(\W\K[@:][A-Za-z]\w*) 0:identifier
addhl -group /moon/code regex \b(and|break|catch|class|continue|do|else(if)?|export|extends|false|finally|for|from|if|import|in|local|nil|not|or|return|super|switch|then|true|try|unless|using|when|while|with)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def moon-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ %sh{
    case $kak_buffile in
        *spec/*_spec.moon)
            altfile=$(eval printf %s\\n $(printf %s\\n $kak_buffile | sed s+spec/+'*'/+';'s/_spec//))
            [ ! -f $altfile ] && echo "echo -color Error 'implementation file not found'" && exit
        ;;
        *.moon)
            path=$kak_buffile
            dirs=$(while [ $path ]; do printf %s\\n $path; path=${path%/*}; done | tail -n +2)
            for dir in $dirs; do
                altdir=$dir/spec
                if [ -d $altdir ]; then
                    altfile=$altdir/$(realpath $kak_buffile --relative-to $dir | sed s+[^/]'*'/++';'s/.moon$/_spec.moon/)
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

def -hidden _moon_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h + $ <ret> d }
    }
}

def -hidden _moon_indent_on_char %{
    eval -draft -itersel %{
        # align _else_ statements to start
        try %{ exec -draft <a-x> <a-k> ^ \h * (else(if)?) $ <ret> <a-\;> <a-?> ^ \h * (if|unless|when) <ret> s \A | \Z <ret> \' <a-&> }
        # align _when_ to _switch_ then indent
        try %{ exec -draft <a-x> <a-k> ^ \h * (when) $ <ret> <a-\;> <a-?> ^ \h * (switch) <ret> s \A | \Z <ret> \' <a-&> \' <space> <gt> }
        # align _catch_ and _finally_ to _try_
        try %{ exec -draft <a-x> <a-k> ^ \h * (catch|finally) $ <ret> <a-\;> <a-?> ^ \h * (try) <ret> s \A | \Z <ret> \' <a-&> }
    }
}

def -hidden _moon_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft K <a-&> }
        # filter previous line
        try %{ exec -draft k : _moon_filter_around_selections <ret> }
        # copy -- comment prefix and following white spaces
        try %{ exec -draft k x s ^ \h * \K -- \h * <ret> y j p }
        # indent after start structure
        try %{ exec -draft k x <a-k> ^ \h * (class|else(if)?|for|if|switch|unless|when|while|with) \b | ([:=]|[-=]>) $ <ret> j <a-gt> }
        # deindent after return statements
        try %{ exec -draft k x <a-k> ^ \h * (break|return) \b <ret> j <a-lt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group moon-highlight global WinSetOption filetype=moon %{ addhl ref moon }

hook global WinSetOption filetype=moon %{
    hook window InsertEnd  .* -group moon-hooks  _moon_filter_around_selections
    hook window InsertChar .* -group moon-indent _moon_indent_on_char
    hook window InsertChar \n -group moon-indent _moon_indent_on_new_line

    alias window alt moon-alternative-file

    set window comment_line_chars '--'
    set window comment_selection_chars ''
}

hook -group moon-highlight global WinSetOption filetype=(?!moon).* %{ rmhl moon }

hook global WinSetOption filetype=(?!moon).* %{
    rmhooks window moon-indent
    rmhooks window moon-hooks

    unalias window alt moon-alternative-file
}
