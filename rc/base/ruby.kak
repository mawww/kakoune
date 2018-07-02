# http://ruby-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*(([.](rb))|(irbrc)|(pryrc)|(Capfile|[.]cap)|(Gemfile|[.]gemspec)|(Guardfile)|(Rakefile|[.]rake)|(Thorfile|[.]thor)|(Vagrantfile)) %{
    set-option buffer filetype ruby
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ruby regions
add-highlighter shared/ruby/code default-region group
add-highlighter shared/ruby/double_string region '"' (?<!\\)(\\\\)*"        regions
add-highlighter shared/ruby/single_string region "'" (?<!\\)(\\\\)*'        fill string
add-highlighter shared/ruby/backtick      region '`' (?<!\\)(\\\\)*`        regions
add-highlighter shared/ruby/regex         region '/' (?<!\\)(\\\\)*/[imox]* regions
add-highlighter shared/ruby/              region '#' '$'                    fill comment
add-highlighter shared/ruby/              region ^begin= ^=end              fill comment
add-highlighter shared/ruby/              region -recurse \( '%[iqrswxIQRSWX]\(' \) fill meta
add-highlighter shared/ruby/              region -recurse \{ '%[iqrswxIQRSWX]\{' \} fill meta
add-highlighter shared/ruby/              region -recurse \[ '%[iqrswxIQRSWX]\[' \] fill meta
add-highlighter shared/ruby/              region -recurse  < '%[iqrswxIQRSWX]<'   > fill meta
add-highlighter shared/ruby/heredoc region '<<[-~]?(\w+)'      '^\h*(\w+)$' fill string
add-highlighter shared/ruby/division region '[\w\)\]](/|(\h+/\h+))' '\w' group # Help Kakoune to better detect /…/ literals

# Regular expression flags are: i → ignore case, m → multi-lines, o → only interpolate #{} blocks once, x → extended mode (ignore white spaces)
# Literals are: i → array of symbols, q → string, r → regular expression, s → symbol, w → array of words, x → capture shell result

add-highlighter shared/ruby/double_string/ default-region fill string
add-highlighter shared/ruby/double_string/interpolation region -recurse \{ \Q#{ \} fill meta

add-highlighter shared/ruby/backtick/ default-region fill meta
add-highlighter shared/ruby/backtick/interpolation region -recurse \{ \Q#{ \} fill meta

add-highlighter shared/ruby/regex/ default-region fill meta
add-highlighter shared/ruby/regex/interpolation region -recurse \{ \Q#{ \} fill meta

add-highlighter shared/ruby/code/ regex \b([A-Za-z]\w*:(?!:))|([$@][A-Za-z]\w*)|((?<!:):(([A-Za-z]\w*[=?!]?)|(\[\]=?)))|([A-Z]\w*|^|\h)\K::(?=[A-Z]) 0:variable

evaluate-commands %sh{
    # Grammar
    # Keywords are collected searching for keywords at
    # https://github.com/ruby/ruby/blob/trunk/parse.y
    keywords="alias|and|begin|break|case|class|def|defined|do|else|elsif|end"
    keywords="${keywords}|ensure|false|for|if|in|module|next|nil|not|or|private|protected|public|redo"
    keywords="${keywords}|rescue|retry|return|self|super|then|true|undef|unless|until|when|while|yield"
    attributes="attr_reader|attr_writer|attr_accessor"
    values="false|true|nil"
    meta="require|include|extend"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=ruby %{
        set-option window static_words ${keywords} ${attributes} ${values} ${meta}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/ruby/code/ regex \b(${keywords})\b 0:keyword
        add-highlighter shared/ruby/code/ regex \b(${attributes})\b 0:attribute
        add-highlighter shared/ruby/code/ regex \b(${values})\b 0:value
        add-highlighter shared/ruby/code/ regex \b(${meta})\b 0:meta
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command ruby-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ evaluate-commands %sh{
    case $kak_buffile in
        *spec/*_spec.rb)
            altfile=$(eval echo $(echo $kak_buffile | sed s+spec/+'*'/+';'s/_spec//))
            [ ! -f $altfile ] && echo "echo -markup '{Error}implementation file not found'" && exit
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
            [ ! -d $altdir ] && echo "echo -markup '{Error}spec/ not found'" && exit
        ;;
        *)
            echo "echo -markup '{Error}alternative file not found'" && exit
        ;;
    esac
    echo "edit $altfile"
}}

define-command -hidden ruby-filter-around-selections %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys <a-x>
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden ruby-indent-on-char %{
    evaluate-commands -no-hooks -draft -itersel %{
        # align middle and end structures to start
        try %{ execute-keys -draft <a-x> <a-k> ^ \h * (else|elsif) $ <ret> <a-\;> <a-?> ^ \h * (if)                                                       <ret> s \A | \z <ret> ) <a-&> }
        try %{ execute-keys -draft <a-x> <a-k> ^ \h * (when)       $ <ret> <a-\;> <a-?> ^ \h * (case)                                                     <ret> s \A | \z <ret> ) <a-&> }
        try %{ execute-keys -draft <a-x> <a-k> ^ \h * (rescue)     $ <ret> <a-\;> <a-?> ^ \h * (begin)                                                    <ret> s \A | \z <ret> ) <a-&> }
        try %{ execute-keys -draft <a-x> <a-k> ^ \h * (end)        $ <ret> <a-\;> <a-?> ^ \h * (begin|case|class|def|do|for|if|module|unless|until|while) <ret> s \A | \z <ret> ) <a-&> }
    }
}

define-command -hidden ruby-indent-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : ruby-filter-around-selections <ret> }
        # indent after start structure
        try %{ execute-keys -draft k <a-x> <a-k> ^ \h * (begin|case|class|def|do|else|elsif|ensure|for|if|module|rescue|unless|until|when|while) \b <ret> j <a-gt> }
    }
}

define-command -hidden ruby-insert-on-new-line %[
    evaluate-commands -no-hooks -draft -itersel %[
        # copy _#_ comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s '^\h*\K#\h*' <ret> y gh j P }
        # wisely add end structure
        evaluate-commands -save-regs x %[
            try %{ execute-keys -draft k <a-x> s ^ \h + <ret> \" x y } catch %{ reg x '' }
            try %[
                evaluate-commands -draft %[
                    execute-keys -draft k<a-x> <a-k>^<c-r>x(begin|case|class|def|do|for|if|module|unless|until|while)<ret> # Check if previous line opens a block
                    # Check that we do not already have an end for this indent level, or that we have another block opening at that indent level first
                    execute-keys -draft Ge <a-K>\A(^\n|^<c-r>x(?!begin)(?!case)(?!class)(?!def)(?!do)(?!for)(?!if)(?!module)(?!unless)(?!until)(?!while)[^\n]*\n)+<c-r>xend$<ret>
                ]
                execute-keys -draft o<c-r>xend<esc> # insert a new line with containing end
            ]
        ]
    ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group ruby-highlight global WinSetOption filetype=ruby %{ add-highlighter window/ruby ref ruby }

hook global WinSetOption filetype=ruby %{
    hook window InsertChar .* -group ruby-indent ruby-indent-on-char
    hook window InsertChar \n -group ruby-insert ruby-insert-on-new-line
    hook window InsertChar \n -group ruby-indent ruby-indent-on-new-line

    alias window alt ruby-alternative-file
}

hook -group ruby-highlight global WinSetOption filetype=(?!ruby).* %{ remove-highlighter window/ruby }

hook global WinSetOption filetype=(?!ruby).* %{
    remove-hooks window ruby-indent
    remove-hooks window ruby-insert

    unalias window alt ruby-alternative-file
}
