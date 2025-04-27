# http://moonscript.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](moon) %{
    set-option buffer filetype moon
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=moon %{
    require-module moon

    hook window ModeChange pop:insert:.* -group moon-trim-indent moon-trim-indent
    hook window InsertChar .* -group moon-indent moon-indent-on-char
    hook window InsertChar \n -group moon-insert moon-insert-on-new-line
    hook window InsertChar \n -group moon-indent moon-indent-on-new-line

    alias window alt moon-alternative-file

    hook -once -always window WinSetOption filetype=.* %{
        remove-hooks window moon-.+
        unalias window alt moon-alternative-file
    }
}

hook -group moon-highlight global WinSetOption filetype=moon %{
    add-highlighter window/moon ref moon
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/moon }
}


provide-module moon %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/moon regions
add-highlighter shared/moon/code default-region group
add-highlighter shared/moon/double_string region '"'  (?<!\\)(\\\\)*" regions
add-highlighter shared/moon/single_string region "'"  (?<!\\)(\\\\)*' fill string
add-highlighter shared/moon/comment       region '--' '$'             fill comment

add-highlighter shared/moon/double_string/base default-region fill string
add-highlighter shared/moon/double_string/interpolation region -recurse \{ \Q#{ \} fill meta

add-highlighter shared/moon/code/ regex \\\w+ 0:function
add-highlighter shared/moon/code/ regex [\W\)\}]\h+\K\.\w+ 0:function
add-highlighter shared/moon/code/ regex (\+|-|\*|/|%|\^|==?|[~!]=|<=?|>=?|\.\.\.?|#|!) 0:operator
add-highlighter shared/moon/code/ regex [-=]> 0:function
add-highlighter shared/moon/code/ regex \b\w+: 0:variable
add-highlighter shared/moon/code/ regex \w+\h*(?=[\(!]) 0:function
add-highlighter shared/moon/code/ regex (?<!\w)[@:]\w+ 0:variable
add-highlighter shared/moon/code/ regex (?<!\w)[@:]__(name|class|inherited):? 0:meta
add-highlighter shared/moon/code/ regex (?<!\w)@@(\w+:?)? 0:meta
add-highlighter shared/moon/code/ regex (\w+)\h*=\h*(?:\(.*?\)\h*)?[-=]> 1:function
add-highlighter shared/moon/code/ regex \b(and|break|class|continue|do|else(if)?|export|extends|for|from|if|import|in|local|not|or|return|switch|then|unless|using|when|while|with)\b 0:keyword
add-highlighter shared/moon/code/ regex \b(true|false|nil|super|self)\b 0:value
add-highlighter shared/moon/code/ regex \b([0-9]+(:?\.[0-9])?(:?[eE]-?[0-9]+)?|0x[0-9a-fA-F]+)\b 0:value
add-highlighter shared/moon/code/ regex class(\h+\w+)?(?:\h+extends(\h+\w+))?\h*$ 1:type 2:attribute
add-highlighter shared/moon/code/ regex \b(_G|_ENV)\b 0:module
add-highlighter shared/moon/code/ regex ^\h*export\h+[\*^]\h*$ 0:meta
add-highlighter shared/moon/code/ regex ^\h*local\h+\*\h*$ 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

define-command moon-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ evaluate-commands %sh{
    case $kak_buffile in
        *spec/*_spec.moon)
            altfile=$(eval printf %s\\n $(printf %s\\n $kak_buffile | sed s+spec/+'*'/+';'s/_spec//))
            [ ! -f $altfile ] && echo "fail 'implementation file not found'" && exit
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
            [ ! -d $altdir ] && echo "fail 'spec/ not found'" && exit
        ;;
        *)
            echo "fail 'alternative file not found'" && exit
        ;;
    esac
    printf %s\\n "edit $altfile"
}}

define-command -hidden moon-trim-indent %{
    evaluate-commands -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden moon-indent-on-char %{
    evaluate-commands -draft -itersel %{
        # align _else_ statements to start
        try %{ execute-keys -draft x <a-k> ^ \h * (else(if)?) $ <ret> <a-semicolon> <a-?> ^ \h * (if|unless|when) <ret> s \A | \z <ret> ) <a-&> }
        # align _when_ to _switch_ then indent
        try %{ execute-keys -draft x <a-k> ^ \h * (when) $ <ret> <a-semicolon> <a-?> ^ \h * (switch) <ret> s \A | \z <ret> ) <a-&> ) , <gt> }
    }
}

define-command -hidden moon-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K--\h* <ret> y gh j P }
    }
}

define-command -hidden moon-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : moon-trim-indent <ret> }
        # indent after start structure
        try %{ execute-keys -draft k x <a-k> ^ \h * (class|else(if)?|for|if|switch|unless|when|while|with) \b | ([:=]|[-=]>) $ <ret> j <a-gt> }
        # deindent after return statements
        try %{ execute-keys -draft k x <a-k> ^ \h * (break|return) \b <ret> j <a-lt> }
    }
}

]
