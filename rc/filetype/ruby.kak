# http://ruby-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*(([.](rb))|(irbrc)|(pryrc)|(Brewfile)|(Capfile|[.]cap)|(Gemfile|[.]gemspec)|(Guardfile)|(Rakefile|[.]rake)|(Thorfile|[.]thor)|(Vagrantfile)) %{
    set-option buffer filetype ruby
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ruby %{
    require-module ruby

    set-option window static_words %opt{ruby_static_words}

    hook window ModeChange pop:insert:.* -group ruby-trim-indent ruby-trim-indent
    hook window InsertChar .* -group ruby-indent ruby-indent-on-char
    hook window InsertChar \n -group ruby-indent ruby-indent-on-new-line
    hook window InsertChar \n -group ruby-insert ruby-insert-on-new-line

    alias window alt ruby-alternative-file

    hook -once -always window WinSetOption filetype=.* %{
        remove-hooks window ruby-.+
        unalias window alt ruby-alternative-file
    }
}

hook -group ruby-highlight global WinSetOption filetype=ruby %{
    add-highlighter window/ruby ref ruby
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ruby }
}

provide-module ruby %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ruby regions
add-highlighter shared/ruby/code default-region group
add-highlighter shared/ruby/double_symbol region ':"' (?<!\\)(\\\\)*"                regions
add-highlighter shared/ruby/single_symbol region ":'" (?<!\\)(\\\\)*'                fill variable
add-highlighter shared/ruby/double_string region '"' (?<!\\)(\\\\)*"                 regions
add-highlighter shared/ruby/single_string region "'" (?<!\\)(\\\\)*'                 fill string
add-highlighter shared/ruby/backtick      region '(?<![$:])`' (?<!\\)(\\\\)*`        regions
add-highlighter shared/ruby/regex         region '(?<![$:])/' (?<!\\)(\\\\)*/[imox]* regions
add-highlighter shared/ruby/              region '#' '$'                             fill comment
add-highlighter shared/ruby/              region ^=begin ^=end                       fill comment
add-highlighter shared/ruby/              region -recurse \( '%[qwQW]?\(' \)         fill string
add-highlighter shared/ruby/              region -recurse \{ '%[qwQW]?\{' \}         fill string
add-highlighter shared/ruby/              region -recurse \[ '%[qwQW]?\[' \]         fill string
add-highlighter shared/ruby/              region -recurse  < '%[qwQW]?<'   >         fill string
add-highlighter shared/ruby/              region -recurse \( '%[isIS]\('  \)         fill variable
add-highlighter shared/ruby/              region -recurse \{ '%[isIS]\{'  \}         fill variable
add-highlighter shared/ruby/              region -recurse \[ '%[isIS]\['  \]         fill variable
add-highlighter shared/ruby/              region -recurse  < '%[isIS]<'    >         fill variable
add-highlighter shared/ruby/              region -recurse \( '%[rxRX]\('  \)         fill meta
add-highlighter shared/ruby/              region -recurse \{ '%[rxRX]\{'  \}         fill meta
add-highlighter shared/ruby/              region -recurse \[ '%[rxRX]\['  \]         fill meta
add-highlighter shared/ruby/              region -recurse  < '%[rxRX]<'    >         fill meta
add-highlighter shared/ruby/              region -match-capture '%[qwQW]?([^\s0-9A-Za-z\(\{\[<>\]\}\)])' ([^\s0-9A-Za-z\(\{\[<>\]\}\)]) fill string
add-highlighter shared/ruby/              region -match-capture '%[isIS]([^\s0-9A-Za-z\(\{\[<>\]\}\)])' ([^\s0-9A-Za-z\(\{\[<>\]\}\)]) fill variable
add-highlighter shared/ruby/              region -match-capture '%[rxRX]([^\s0-9A-Za-z\(\{\[<>\]\}\)])' ([^\s0-9A-Za-z\(\{\[<>\]\}\)]) fill meta
add-highlighter shared/ruby/heredoc region -match-capture '<<[-~]?(?!self)(\w+)'      '^\h*(\w+)$' fill string
add-highlighter shared/ruby/division region '[\w\)\]]\K(/|(\h+/\h+))' '\w' group # Help Kakoune to better detect /…/ literals

# Regular expression flags are: i → ignore case, m → multi-lines, o → only interpolate #{} blocks once, x → extended mode (ignore white spaces)
# Literals are: i → array of symbols, q → string, r → regular expression, s → symbol, w → array of words, x → capture shell result

add-highlighter shared/ruby/double_string/ default-region fill string
add-highlighter shared/ruby/double_string/interpolation region -recurse \{ \Q#{ \} fill meta

add-highlighter shared/ruby/double_symbol/ default-region fill variable
add-highlighter shared/ruby/double_symbol/interpolation region -recurse \{ \Q#{ \} fill meta

add-highlighter shared/ruby/backtick/ default-region fill meta
add-highlighter shared/ruby/backtick/interpolation region -recurse \{ \Q#{ \} fill meta

add-highlighter shared/ruby/regex/ default-region fill meta
add-highlighter shared/ruby/regex/interpolation region -recurse \{ \Q#{ \} fill meta

evaluate-commands %sh{
    # Grammar
    # Keywords are collected searching for keywords at
    # https://github.com/ruby/ruby/blob/trunk/parse.y
    keywords="alias|and|begin|break|case|class|def|defined|do|else|elsif|end"
    keywords="${keywords}|ensure|false|for|if|in|module|next|nil|not|or|private|protected|public|redo"
    keywords="${keywords}|rescue|retry|return|self|super|then|true|undef|unless|until|when|while|yield"
    attributes="attr_reader|attr_writer|attr_accessor"
    values="false|true|nil"
    meta="require|require_relative|include|extend"

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list ruby_static_words ${keywords} ${attributes} ${values} ${meta}" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/ruby/code/ regex \b(${keywords})[^0-9A-Za-z_!?] 1:keyword
        add-highlighter shared/ruby/code/ regex \b(${attributes})\b 0:attribute
        add-highlighter shared/ruby/code/ regex \b(${values})\b 0:value
        add-highlighter shared/ruby/code/ regex \b(${meta})\b 0:meta
    "
}

add-highlighter shared/ruby/code/ regex \b(\w+:(?!:))|(:?(\$(-[0FIKWadilpvw]|["'`/~&+=!$*,:.\;<>?@\\])|(\$|@@?)\w+))|((?<!:):(![~=]|=~|>[=>]?|<((=>?)|<)?|[+\-]@?|\*\*?|===?|[/`%&!^|~]|(\w+[=?!]?)|(\[\]=?)))|([A-Z]\w*|^|\h)\K::(?=[A-Z]) 0:variable

# Commands
# ‾‾‾‾‾‾‾‾

define-command ruby-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ evaluate-commands %sh{
    case $kak_buffile in
        *spec/*_spec.rb)
            altfile=$(eval echo $(echo $kak_buffile | sed s+spec/+'*'/+';'s/_spec//))
            [ ! -f $altfile ] && echo "fail 'implementation file not found'" && exit
        ;;
        *test/*_test.rb)
            altfile=$(eval echo $(echo $kak_buffile | sed s+test/+'*'/+';'s/_test//))
            [ ! -f $altfile ] && echo "fail 'implementation file not found'" && exit
        ;;
        *.rb)
            altfile=""
            altdir=""
            path=$kak_buffile
            dirs=$(while [ $path ]; do echo $path; path=${path%/*}; done | tail -n +2)
            for dir in $dirs; do
                altdir=$dir/spec && suffix=spec
                [ ! -d $altdir ] && altdir=$dir/test && suffix=test
                if [ -d $altdir ]; then
                    altfile=$altdir/$(realpath $kak_buffile --relative-to $dir | sed s+[^/]'*'/++';'s/.rb$/_${suffix}.rb/)
                    break
                fi
            done
            [ ! -d "$altdir" ] && echo "fail 'spec/ and test/ not found'" && exit
        ;;
        *)
            echo "fail 'alternative file not found'" && exit
        ;;
    esac
    echo "edit $altfile"
}}

define-command -hidden ruby-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden ruby-indent-on-char %{
    evaluate-commands -no-hooks -draft -itersel %{
        # align middle and end structures to start
        try %{ execute-keys -draft x <a-k> ^ \h * (else)   $ <ret> <a-a> i <a-semicolon> <a-?> ^ \h * (if|case)                                               <ret> <a-S> 1<a-&> }
        try %{ execute-keys -draft x <a-k> ^ \h * (elsif)  $ <ret> <a-a> i <a-semicolon> <a-?> ^ \h * (if)                                                    <ret> <a-S> 1<a-&> }
        try %{ execute-keys -draft x <a-k> ^ \h * (when)   $ <ret> <a-a> i <a-semicolon> <a-?> ^ \h * (case)                                                  <ret> <a-S> 1<a-&> }
        try %{ execute-keys -draft x <a-k> ^ \h * (rescue) $ <ret> <a-a> i <a-semicolon> <a-?> ^ \h * (begin|def)                                             <ret> <a-S> 1<a-&> }
    }
}

define-command -hidden ruby-indent-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : ruby-trim-indent <ret> }
        # indent after start structure
        try %{ execute-keys -draft k x <a-k> ^ \h * (begin|case|class|def|else|elsif|ensure|for|if|module|rescue|unless|until|when|while|.+\bdo$|.+\bdo\h\|.+(?=\|)) [^0-9A-Za-z_!?] <ret> j <a-gt> }
    }
}

define-command -hidden ruby-insert-on-new-line %[
    evaluate-commands -no-hooks -draft -itersel %[
        # copy _#_ comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K#\h* <ret> y jgi P }
        # wisely add end structure
        evaluate-commands -save-regs x %[
            try %{ execute-keys -draft k x s ^ \h + <ret> \" x y } catch %{ reg x '' }
            try %[
                evaluate-commands -draft %[
                    # Check if previous line opens a block
                    execute-keys -draft kx <a-k>^<c-r>x(begin|case|class|def|for|if|module|unless|until|while|.+\bdo$|.+\bdo\h\|.+(?=\|))[^0-9A-Za-z_!?]<ret>
                    # Check that we do not already have an end for this indent level which is first set via `ruby-indent-on-new-line` hook
                    execute-keys -draft }i J x <a-K> ^<c-r>x(end|else|elsif|rescue|when)[^0-9A-Za-z_!?]<ret>
                ]
                execute-keys -draft o<c-r>xend<esc> # insert a new line with containing end
            ]
        ]
    ]
]

§
