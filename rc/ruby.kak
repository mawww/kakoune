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
    single_string "'" "'"                    '' \
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

addhl -group /ruby/code regex \<([A-Za-z]\w+:)|([$@][A-Za-z]\w+)|(\W\K:[A-Za-z]\w+[=?!]?) 0:identifier
addhl -group /ruby/code regex \<(require|include)\> 0:meta
addhl -group /ruby/code regex \<(attr_(reader|writer|accessor))\> 0:attribute

# Keywords are collected searching for keyword_ at
# https://github.com/ruby/ruby/blob/trunk/parse.y
addhl -group /ruby/code regex \<(alias|and|begin|break|case|class|def|defined|do|else|elsif|end|ensure|false|for|if|in|module|next|nil|not|or|redo|rescue|retry|return|self|super|then|true|undef|unless|until|when|while|yield)\> 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _ruby_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _ruby_indent_on_char %<
    eval -draft -itersel %<
        # deindent on (else|elsif|end|rescue|when) keyword insertion
        try %{ exec -draft <space> <a-i>w <a-k> (else|elsif|end|rescue|when) <ret> <a-lt> }
        # align closer token to its opener when alone on a line
        try %< exec -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \`|.\' <ret> 1<a-&> >
    >
>

def -hidden _ruby_indent_on_new_line %(
    eval -draft -itersel %(
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _ruby_filter_around_selections <ret> }
        # copy _#_ comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y j p }
        # indent after (else|elsif|rescue|when) keywords and lines beginning / ending with opener token
        try %( exec -draft <space> k x <a-k> (else|elsif|rescue|when)|^\h*[[{]|[[{]$ <ret> j <a-gt> )
        # indent after (begin|case|class|def|do|if|loop|module|unless|until|while) keywords and add _end_ keyword
        try %{ exec -draft <space> k x <a-k> (begin|case|class|def|do|(?<!els)if|loop|module|unless|until|while) <ret> x y p j a end <esc> k <a-gt> }
    )
)

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ruby %{
    addhl ref ruby

    hook window InsertEnd  .* -group ruby-hooks  _ruby_filter_around_selections
    hook window InsertChar .* -group ruby-indent _ruby_indent_on_char
    hook window InsertChar \n -group ruby-indent _ruby_indent_on_new_line

    set window comment_line_chars '#'
    set window comment_selection_chars '^begin=:^=end'
}

hook global WinSetOption filetype=(?!ruby).* %{
    rmhl ruby
    rmhooks window ruby-indent
    rmhooks window ruby-hooks
}
