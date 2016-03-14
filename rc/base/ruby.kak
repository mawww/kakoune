# http://ruby-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require commenting.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-ruby %{
    set buffer filetype ruby
}

hook global BufCreate .*(([.](rb))|(irbrc)|(pryrc)|(Capfile|[.]cap)|(Gemfile)|(Guardfile)|(Rakefile|[.]rake)|(Thorfile|[.]thor)|(Vagrantfile)) %{
    set buffer filetype ruby
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code ruby       \
    double_string '"' (?<!\\)(\\\\)*"        '' \
    single_string "'" (?<!\\)(\\\\)*'        '' \
    backtick      '`' (?<!\\)(\\\\)*`        '' \
    regex         '/' (?<!\\)(\\\\)*/[imox]* '' \
    comment       '#' '$'                    '' \
    comment       ^begin= ^=end              '' \
    literal       '%[iqrswxIQRSWX]\(' \)     \( \
    literal       '%[iqrswxIQRSWX]\{' \}     \{ \
    literal       '%[iqrswxIQRSWX]\[' \]     \[ \
    literal       '%[iqrswxIQRSWX]<'   >      <

# Regular expression flags are: i → ignore case, m → multi-lines, o → only interpolate #{} blocks once, x → extended mode (ignore white spaces)
# Literals are: i → array of symbols, q → string, r → regular expression, s → symbol, w → array of words, x → capture shell result

addhl -group /ruby/double_string fill string
addhl -group /ruby/double_string regions regions interpolation \Q#{ \} \{
addhl -group /ruby/double_string/regions/interpolation fill meta

addhl -group /ruby/single_string fill string

addhl -group /ruby/backtick fill meta
addhl -group /ruby/backtick regions regions interpolation \Q#{ \} \{
addhl -group /ruby/backtick/regions/interpolation fill meta

addhl -group /ruby/regex fill meta
addhl -group /ruby/regex regions regions interpolation \Q#{ \} \{
addhl -group /ruby/regex/regions/interpolation fill meta

addhl -group /ruby/comment fill comment

addhl -group /ruby/literal fill meta

addhl -group /ruby/code regex \<([A-Za-z]\w*:)|([$@][A-Za-z]\w*)|(\W\K:[A-Za-z]\w*[=?!]?) 0:identifier

%sh{
    # Grammar
    # Keywords are collected searching for keyword_ at
    # https://github.com/ruby/ruby/blob/trunk/parse.y
    keywords="alias:and:begin:break:case:class:def:defined:do:else:elsif:end"
    keywords="${keywords}:ensure:false:for:if:in:module:next:nil:not:or:redo"
    keywords="${keywords}:rescue:retry:return:self:super:then:true:undef:unless:until:when:while:yield"
    attributes="attr_reader:attr_writer:attr_accessor"
    values="false:true:nil"
    meta="require:include"

    # Add the language's grammar to the static completion list
    echo "hook global WinSetOption filetype=ruby %{
        set window static_words '${keywords}'
        set -add window static_words '${attributes}'
        set -add window static_words '${values}'
        set -add window static_words '${meta}'
    }"

    # Highlight keywords
    echo "
        addhl -group /ruby/code regex \<(${keywords//:/|})\> 0:keyword
        addhl -group /ruby/code regex \<(${attributes//:/|})\> 0:attribute
        addhl -group /ruby/code regex \<(${values//:/|})\> 0:value
        addhl -group /ruby/code regex \<(${meta//:/|})\> 0:meta
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _ruby_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h + $ <ret> d }
    }
}

def -hidden _ruby_indent_on_char %{
    eval -draft -itersel %{
        # align middle and end structures to start
        try %{ exec -draft <a-x> <a-k> ^ \h * (else|elsif) $ <ret> <a-\;> <a-?> ^ \h * (if)                                                       <ret> s \A | \Z <ret> \' <a-&> }
        try %{ exec -draft <a-x> <a-k> ^ \h * (when)       $ <ret> <a-\;> <a-?> ^ \h * (case)                                                     <ret> s \A | \Z <ret> \' <a-&> }
        try %{ exec -draft <a-x> <a-k> ^ \h * (rescue)     $ <ret> <a-\;> <a-?> ^ \h * (begin)                                                    <ret> s \A | \Z <ret> \' <a-&> }
        try %{ exec -draft <a-x> <a-k> ^ \h * (end)        $ <ret> <a-\;> <a-?> ^ \h * (begin|case|class|def|do|for|if|module|unless|until|while) <ret> s \A | \Z <ret> \' <a-&> }
    }
}

def -hidden _ruby_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft K <a-&> }
        # filter previous line
        try %{ exec -draft k : _ruby_filter_around_selections <ret> }
        # indent after start structure
        try %{ exec -draft k x <a-k> ^ \h * (begin|case|class|def|do|else|elsif|ensure|for|if|module|rescue|unless|until|when|while) \b <ret> j <a-gt> }
    }
}

def -hidden _ruby_insert_on_new_line %{
    eval -draft -itersel %{
        # copy _#_ comment prefix and following white spaces
        try %{ exec -draft k x s ^ \h * \K \# \h * <ret> y j p }
        # wisely add end structure
        eval -save-regs x %{
            try %{ exec -draft k x s ^ \h + <ret> \" x y } catch %{ reg x '' }
            try %{ exec -draft k x <a-k> ^ <c-r> x (begin|case|class|def|do|for|if|module|unless|until|while) <ret> j <a-a> i X <a-\;> K <a-K> ^ <c-r> x (for|function|if|while) . * \n <c-r> x end $ <ret> j x y p j a end <esc> <a-lt> }
        }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ruby %{
    addhl ref ruby

    hook window InsertChar .* -group ruby-indent _ruby_indent_on_char
    hook window InsertChar \n -group ruby-indent _ruby_indent_on_new_line
    hook window InsertChar \n -group ruby-insert _ruby_insert_on_new_line

    set window comment_line_chars '#'
    set window comment_selection_chars '^begin=:^=end'

    # Rubocop requires a filepath that will be used when generating the errors summary,
    # even though it's reading anonymous data on stdin
    # It also leaves an ugly separator on the first line on the output
    set window formatcmd 'rubocop --auto-correct --stdin - -o /dev/null | sed 1d'
}

hook global WinSetOption filetype=(?!ruby).* %{
    rmhl ruby
    rmhooks window ruby-indent
    rmhooks window ruby-hooks
}
