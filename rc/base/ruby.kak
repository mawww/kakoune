# http://ruby-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*(([.](rb))|(irbrc)|(pryrc)|(Capfile|[.]cap)|(Gemfile)|(Guardfile)|(Rakefile|[.]rake)|(Thorfile|[.]thor)|(Vagrantfile)) %{
    set buffer filetype ruby
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code ruby       \
    double_string '"' (?<!\\)(\\\\)*"        '' \
    single_string "'" (?<!\\)(\\\\)*'        '' \
    backtick      '`' (?<!\\)(\\\\)*`        '' \
    regex         '/' (?<!\\)(\\\\)*/[imox]* '' \
    comment       '#' '$'                    '' \
    comment       ^begin= ^=end              '' \
    literal       '%[iqrswxIQRSWX]\(' \)     \( \
    literal       '%[iqrswxIQRSWX]\{' \}     \{ \
    literal       '%[iqrswxIQRSWX]\[' \]     \[ \
    literal       '%[iqrswxIQRSWX]<'   >      < \
    division '[\w\)\]](/|(\h+/\h+))' '\w' '' # Help Kakoune to better detect /…/ literals

# Regular expression flags are: i → ignore case, m → multi-lines, o → only interpolate #{} blocks once, x → extended mode (ignore white spaces)
# Literals are: i → array of symbols, q → string, r → regular expression, s → symbol, w → array of words, x → capture shell result

add-highlighter -group /ruby/double_string fill string
add-highlighter -group /ruby/double_string regions regions interpolation \Q#{ \} \{
add-highlighter -group /ruby/double_string/regions/interpolation fill meta

add-highlighter -group /ruby/single_string fill string

add-highlighter -group /ruby/backtick fill meta
add-highlighter -group /ruby/backtick regions regions interpolation \Q#{ \} \{
add-highlighter -group /ruby/backtick/regions/interpolation fill meta

add-highlighter -group /ruby/regex fill meta
add-highlighter -group /ruby/regex regions regions interpolation \Q#{ \} \{
add-highlighter -group /ruby/regex/regions/interpolation fill meta

add-highlighter -group /ruby/comment fill comment

add-highlighter -group /ruby/literal fill meta

add-highlighter -group /ruby/code regex \b([A-Za-z]\w*:(?=[^:]))|([$@][A-Za-z]\w*)|((?<=[^:]):[A-Za-z]\w*[=?!]?)|([A-Z]\w*|^|\h)\K::(?=[A-Z]) 0:identifier

%sh{
    # Grammar
    # Keywords are collected searching for keywords at
    # https://github.com/ruby/ruby/blob/trunk/parse.y
    keywords="alias|and|begin|break|case|class|def|defined|do|else|elsif|end"
    keywords="${keywords}|ensure|false|for|if|in|module|next|nil|not|or|redo"
    keywords="${keywords}|rescue|retry|return|self|super|then|true|undef|unless|until|when|while|yield"
    attributes="attr_reader|attr_writer|attr_accessor"
    values="false|true|nil"
    meta="require|include|extend"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=ruby %{
        set window static_words '${keywords}:${attributes}:${values}:${meta}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        add-highlighter -group /ruby/code regex \b(${keywords})\b 0:keyword
        add-highlighter -group /ruby/code regex \b(${attributes})\b 0:attribute
        add-highlighter -group /ruby/code regex \b(${values})\b 0:value
        add-highlighter -group /ruby/code regex \b(${meta})\b 0:meta
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

def ruby-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ %sh{
    case $kak_buffile in
        *spec/*_spec.rb)
            altfile=$(eval echo $(echo $kak_buffile | sed s+spec/+'*'/+';'s/_spec//))
            [ ! -f $altfile ] && echo "echo -color Error 'implementation file not found'" && exit
        ;;
        *.rb)
            path=$kak_buffile
            dirs=$(while [ $path ]; do echo $path; path=${path%/*}; done | tail -n +2)
            for dir in $dirs; do
                altdir=$dir/spec
                if [ -d $altdir ]; then
                    altfile=$altdir/$(realpath $kak_buffile --relative-to $dir | sed s+[^/]'*'/++';'s/.rb$/_spec.rb/)
                    break
                fi
            done
            [ ! -d $altdir ] && echo "echo -color Error 'spec/ not found'" && exit
        ;;
        *)
            echo "echo -color Error 'alternative file not found'" && exit
        ;;
    esac
    echo "edit $altfile"
}}

def -hidden _ruby_filter_around_selections %{
    eval -no-hooks -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h + $ <ret> d }
    }
}

def -hidden _ruby_indent_on_char %{
    eval -no-hooks -draft -itersel %{
        # align middle and end structures to start
        try %{ exec -draft <a-x> <a-k> ^ \h * (else|elsif) $ <ret> <a-\;> <a-?> ^ \h * (if)                                                       <ret> s \A | \Z <ret> \' <a-&> }
        try %{ exec -draft <a-x> <a-k> ^ \h * (when)       $ <ret> <a-\;> <a-?> ^ \h * (case)                                                     <ret> s \A | \Z <ret> \' <a-&> }
        try %{ exec -draft <a-x> <a-k> ^ \h * (rescue)     $ <ret> <a-\;> <a-?> ^ \h * (begin)                                                    <ret> s \A | \Z <ret> \' <a-&> }
        try %{ exec -draft <a-x> <a-k> ^ \h * (end)        $ <ret> <a-\;> <a-?> ^ \h * (begin|case|class|def|do|for|if|module|unless|until|while) <ret> s \A | \Z <ret> \' <a-&> }
    }
}

def -hidden _ruby_indent_on_new_line %{
    eval -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft K <a-&> }
        # filter previous line
        try %{ exec -draft k : _ruby_filter_around_selections <ret> }
        # indent after start structure
        try %{ exec -draft k x <a-k> ^ \h * (begin|case|class|def|do|else|elsif|ensure|for|if|module|rescue|unless|until|when|while) \b <ret> j <a-gt> }
    }
}

def -hidden _ruby_insert_on_new_line %{
    eval -no-hooks -draft -itersel %{
        # copy _#_ comment prefix and following white spaces
        try %{ exec -draft k X<a-s><a-space> s ^ \h * \# \h * <ret> y gh j P }
        # wisely add end structure
        eval -save-regs x %{
            try %{ exec -draft k X<a-s><a-space> s ^ \h + <ret> \" x y } catch %{ reg x '' }
            try %{ exec -draft k x <a-k> ^ <c-r> x (begin|case|class|def|do|for|if|module|unless|until|while) <ret> j <a-a> i X <a-\;> K <a-K> ^ <c-r> x (begin|case|class|def|do|for|if|module|unless|until|while) . * \n <c-r> x end $ <ret> j x y p j a end <esc> <a-lt> }
        }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group ruby-highlight global WinSetOption filetype=ruby %{ add-highlighter ref ruby }

hook global WinSetOption filetype=ruby %{
    hook window InsertChar .* -group ruby-indent _ruby_indent_on_char
    hook window InsertChar \n -group ruby-indent _ruby_indent_on_new_line
    hook window InsertChar \n -group ruby-insert _ruby_insert_on_new_line

    alias window alt ruby-alternative-file
}

hook -group ruby-highlight global WinSetOption filetype=(?!ruby).* %{ remove-highlighter ruby }

hook global WinSetOption filetype=(?!ruby).* %{
    remove-hooks window ruby-indent
    remove-hooks window ruby-insert

    unalias window alt ruby-alternative-file
}
