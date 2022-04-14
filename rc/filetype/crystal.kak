# Crystal
# https://crystal-lang.org

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate '.*\.cr' %{
    set-option buffer filetype crystal
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=crystal %{
    require-module crystal

    add-highlighter window/crystal ref crystal
    evaluate-commands set-option window static_words %opt{crystal_keywords} %opt{crystal_attributes} %opt{crystal_objects}

    hook window ModeChange pop:insert:.* -group crystal-trim-indent crystal-trim-indent
    hook window InsertChar .*   -group crystal-indent crystal-indent-on-char
    hook window InsertChar '\n' -group crystal-indent crystal-indent-on-new-line
    hook window InsertChar '\n' -group crystal-insert crystal-insert-on-new-line

    hook -always -once window WinSetOption filetype=.* %{
        remove-highlighter window/crystal
        remove-hooks window crystal-.+
    }
}

provide-module crystal %§

declare-option -hidden str-list crystal_keywords 'abstract' 'alias' 'annotation' 'as' 'asm' 'begin' 'break' 'case' 'class' 'def' 'do' 'else' 'elsif' 'end' 'ensure' 'enum' 'extend' 'false' 'for' 'fun' 'if' 'include' 'instance_sizeof' 'is_a?' 'lib' 'macro' 'module' 'next' 'nil' 'nil?' 'of' 'offsetof' 'out' 'pointerof' 'private' 'protected' 'require' 'rescue' 'responds_to?' 'return' 'select' 'self' 'sizeof' 'struct' 'super' 'then' 'true' 'type' 'typeof' 'uninitialized' 'union' 'unless' 'until' 'verbatim' 'when' 'while' 'with' 'yield'
# https://crystal-lang.org/reference/syntax_and_semantics/methods_and_instance_variables.html#getters-and-setters
declare-option -hidden str-list crystal_attributes 'getter' 'setter' 'property'
declare-option -hidden str-list crystal_operators '+' '-' '*' '/' '//' '%' '|' '&' '^' '~' '**' '<<' '<' '<=' '==' '!=' '=~' '!~' '>>' '>' '>=' '<=>' '===' '[]' '[]=' '[]?' '[' '&+' '&-' '&*' '&**'
declare-option -hidden str-list crystal_objects 'Adler32' 'ArgumentError' 'Array' 'Atomic' 'Base64' 'Benchmark' 'BigDecimal' 'BigFloat' 'BigInt' 'BigRational' 'BitArray' 'Bool' 'Box' 'Bytes' 'Channel' 'Char' 'Class' 'Colorize' 'Comparable' 'Complex' 'Concurrent' 'ConcurrentExecutionException' 'CRC32' 'Crypto' 'Crystal' 'CSV' 'Debug' 'Deprecated' 'Deque' 'Digest' 'Dir' 'DivisionByZeroError' 'DL' 'ECR' 'Enum' 'Enumerable' 'ENV' 'Errno' 'Exception' 'Fiber' 'File' 'FileUtils' 'Flags' 'Flate' 'Float' 'Float32' 'Float64' 'GC' 'Gzip' 'Hash' 'HTML' 'HTTP' 'Indexable' 'IndexError' 'INI' 'Int' 'Int128' 'Int16' 'Int32' 'Int64' 'Int8' 'InvalidBigDecimalException' 'InvalidByteSequenceError' 'IO' 'IPSocket' 'Iterable' 'Iterator' 'JSON' 'KeyError' 'Levenshtein' 'Link' 'LLVM' 'Logger' 'Markdown' 'Math' 'MIME' 'Mutex' 'NamedTuple' 'Nil' 'NilAssertionError' 'NotImplementedError' 'Number' 'OAuth' 'OAuth2' 'Object' 'OpenSSL' 'OptionParser' 'OverflowError' 'PartialComparable' 'Path' 'Pointer' 'PrettyPrint' 'Proc' 'Process' 'Random' 'Range' 'Readline' 'Reference' 'Reflect' 'Regex' 'SemanticVersion' 'Set' 'Signal' 'Slice' 'Socket' 'Spec' 'StaticArray' 'String' 'StringPool' 'StringScanner' 'Struct' 'Symbol' 'System' 'TCPServer' 'TCPSocket' 'Termios' 'Time' 'Tuple' 'TypeCastError' 'UDPSocket' 'UInt128' 'UInt16' 'UInt32' 'UInt64' 'UInt8' 'Unicode' 'Union' 'UNIXServer' 'UNIXSocket' 'URI' 'UUID' 'VaList' 'Value' 'WeakRef' 'XML' 'YAML' 'Zip' 'Zlib'

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/crystal regions
add-highlighter shared/crystal/code default-region group

# Comments
# https://crystal-lang.org/reference/syntax_and_semantics/comments.html
# Avoid string literals with interpolation
add-highlighter shared/crystal/comment region '#(?!\{)' '$' fill comment

# String
# https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html
add-highlighter shared/crystal/string region '"' '(?<!\\)(\\\\)*"' regions

# Percent string literals
# https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#percent-string-literals
add-highlighter shared/crystal/parenthesis-string region -recurse '\(' '%Q?\(' '\)' regions
add-highlighter shared/crystal/bracket-string     region -recurse '\[' '%Q?\[' '\]' regions
add-highlighter shared/crystal/brace-string       region -recurse '\{' '%Q?\{' '\}' regions
add-highlighter shared/crystal/angle-string       region -recurse '<' '%Q?<' '>'    regions
add-highlighter shared/crystal/pipe-string        region          '%Q?\|' '\|'      regions

# Raw
# https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#percent-string-literals
# https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#percent-string-array-literal
# https://crystal-lang.org/reference/syntax_and_semantics/literals/symbol.html#percent-symbol-array-literal
add-highlighter shared/crystal/raw-parenthesis-string region -recurse '\(' '%[qwi]\(' '\)' fill string
add-highlighter shared/crystal/raw-bracket-string     region -recurse '\[' '%[qwi]\[' '\]' fill string
add-highlighter shared/crystal/raw-brace-string       region -recurse '\{' '%[qwi]\{' '\}' fill string
add-highlighter shared/crystal/raw-angle-string       region -recurse '<' '%[qwi]<' '>'    fill string
add-highlighter shared/crystal/raw-pipe-string        region          '%[qwi]\|' '\|'      fill string

# Here document
# https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#heredoc
add-highlighter shared/crystal/heredoc region -match-capture '<<-(\w+)' '^\h*(\w+)$' regions
# Raw
add-highlighter shared/crystal/raw-heredoc region -match-capture "<<-'(\w+)'" '^\h*(\w+)$' regions
add-highlighter shared/crystal/raw-heredoc/fill default-region fill string
add-highlighter shared/crystal/raw-heredoc/interpolation region -recurse '\{' '#\{' '\}' fill meta

# Symbol
# https://crystal-lang.org/reference/syntax_and_semantics/literals/symbol.html
add-highlighter shared/crystal/quoted-symbol region ':"' '(?<!\\)(\\\\)*"' fill value

# Regular expressions
# https://crystal-lang.org/reference/syntax_and_semantics/literals/regex.html
# https://crystal-lang.org/reference/syntax_and_semantics/literals/regex.html#modifiers
add-highlighter shared/crystal/regex region '/' '(?<!\\)(\\\\)*/[imx]*' regions
# Avoid unterminated regular expression
add-highlighter shared/crystal/division region ' / ' '.\K' group

# Percent regex literals
# https://crystal-lang.org/reference/syntax_and_semantics/literals/regex.html#percent-regex-literals
add-highlighter shared/crystal/parenthesis-regex region -recurse '\(' '%r?\(' '\)[imx]*' regions
add-highlighter shared/crystal/bracket-regex region -recurse '\[' '%r?\[' '\][imx]*' regions
add-highlighter shared/crystal/brace-regex region -recurse '\{' '%r?\{' '\}[imx]*' regions
add-highlighter shared/crystal/angle-regex region -recurse '<' '%r?<' '>[imx]*' regions
add-highlighter shared/crystal/pipe-regex region '%r?\|' '\|[imx]*' regions

# Command
# https://crystal-lang.org/reference/syntax_and_semantics/literals/command.html
add-highlighter shared/crystal/command region '`' '(?<!\\)(\\\\)*`' regions

# Percent command literals
add-highlighter shared/crystal/parenthesis-command region -recurse '\(' '%x?\(' '\)' regions
add-highlighter shared/crystal/bracket-command region -recurse '\[' '%x?\[' '\]' regions
add-highlighter shared/crystal/brace-command region -recurse '\{' '%x?\{' '\}' regions
add-highlighter shared/crystal/angle-command region -recurse '<' '%x?<' '>' regions
add-highlighter shared/crystal/pipe-command region '%x?\|' '\|' regions

evaluate-commands %sh[
    # Keywords
    eval "set -- $kak_quoted_opt_crystal_keywords"
    regex="\\b(?:\\Q$1\\E"
    shift
    for keyword do
        regex="$regex|\\Q$keyword\\E"
    done
    regex="$regex)\\b"
    printf 'add-highlighter shared/crystal/code/keywords regex %s 0:keyword\n' "$regex"

    # Attributes
    eval "set -- $kak_quoted_opt_crystal_attributes"
    regex="\\b(?:\\Q$1\\E"
    shift
    for attribute do
        regex="$regex|\\Q$attribute\\E"
    done
    regex="$regex)\\b"
    printf 'add-highlighter shared/crystal/code/attributes regex %s 0:attribute\n' "$regex"

    # Symbols
    eval "set -- $kak_quoted_opt_crystal_operators"
    # Avoid to match modules
    regex="(?<!:):(?:\\w+[?!]?"
    for operator do
        regex="$regex|\\Q$operator\\E"
    done
    regex="$regex)"
    printf 'add-highlighter shared/crystal/code/symbols regex %%(%s) 0:value\n' "$regex"

    # Objects
    eval "set -- $kak_quoted_opt_crystal_objects"
    regex="\\b(?:\\Q$1\\E"
    shift
    for object do
        regex="$regex|\\Q$object\\E"
    done
    regex="$regex)\\b"
    printf 'add-highlighter shared/crystal/code/objects regex %s 0:builtin\n' "$regex"

    # Interpolation
    # String
    # https://crystal-lang.org/reference/syntax_and_semantics/literals/string.html#interpolation
    for id in string parenthesis-string bracket-string brace-string angle-string pipe-string heredoc; do
        printf "
            add-highlighter shared/crystal/$id/fill default-region fill string
            add-highlighter shared/crystal/$id/interpolation region -recurse '\\{' '#\\{' '\\}' ref crystal
        "
    done

    # Regular expressions
    # https://crystal-lang.org/reference/syntax_and_semantics/literals/regex.html#interpolation
    for id in regex parenthesis-regex bracket-regex brace-regex angle-regex pipe-regex; do
        printf "
            add-highlighter shared/crystal/$id/fill default-region fill meta
            add-highlighter shared/crystal/$id/interpolation region -recurse '\\{' '#\\{' '\\}' ref crystal
        "
    done

    # Command
    for id in command parenthesis-command bracket-command brace-command angle-command pipe-command; do
        printf "
            add-highlighter shared/crystal/$id/fill default-region fill meta
            add-highlighter shared/crystal/$id/interpolation region -recurse '\\{' '#\\{' '\\}' ref crystal
        "
    done
]

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden crystal-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h+$ <ret> d }
    }
}

define-command -hidden crystal-indent-on-char %{
    evaluate-commands -no-hooks -draft -itersel %{
        # align 'else' to 'if/case'
        try %{ execute-keys -draft x <a-k> ^\h*else$   <ret> <a-a>i <a-semicolon> <a-?> ^\h*(?:if|case)                                               <ret> <a-S> 1<a-&> }
        # align 'elsif' to 'if'
        try %{ execute-keys -draft x <a-k> ^\h*elsif$  <ret> <a-a>i <a-semicolon> <a-?> ^\h*(?:if)                                                    <ret> <a-S> 1<a-&> }
        # align 'when' to 'case'
        try %{ execute-keys -draft x <a-k> ^\h*when$   <ret> <a-a>i <a-semicolon> <a-?> ^\h*(?:case)                                                  <ret> <a-S> 1<a-&> }
        # align 'rescue' to 'begin/def'
        try %{ execute-keys -draft x <a-k> ^\h*rescue$ <ret> <a-a>i <a-semicolon> <a-?> ^\h*(?:begin|def)                                             <ret> <a-S> 1<a-&> }
        # align 'end' to opening structure
        try %{ execute-keys -draft x <a-k> ^\h*end$    <ret> <a-a>i <a-semicolon> <a-?> ^\h*(?:begin|case|class|def|for|if|module|unless|until|while) <ret> <a-S> 1<a-&> }
    }
}

define-command -hidden crystal-indent-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # Copy previous line indent
        try %{ execute-keys -draft K <a-&> }
        # Remove previous line's trailing spaces
        try %{ execute-keys -draft k :crystal-trim-indent <ret> }
        # Indent after start structure/opening statement
        try %{ execute-keys -draft k x <a-k> ^\h*(?:begin|case|class|def|else|elsif|ensure|for|if|module|rescue|unless|until|when|while|.+\bdo$|.+\bdo\h\|.+(?=\|))[^0-9A-Za-z_!?] <ret> j <a-gt> }
    }
}

define-command -hidden crystal-insert-on-new-line %[
    evaluate-commands -no-hooks -draft -itersel %[
        # copy _#_ comment prefix and following white spaces
        try %{ execute-keys -draft k x s '^\h*\K#\h*' <ret> y j x<semicolon> P }
        # wisely add end structure
        evaluate-commands -save-regs x %[
            try %{ execute-keys -draft k x s ^ \h + <ret> \" x y } catch %{ reg x '' }
            try %[
                evaluate-commands -draft %[
                    # Check if previous line opens a block
                    execute-keys -draft kx <a-k>^<c-r>x(?:begin|case|class|def|for|if|module|unless|until|while|.+\bdo$|.+\bdo\h\|.+(?=\|))[^0-9A-Za-z_!?]<ret>
                    # Check that we do not already have an end for this indent level which is first set via `crystal-indent-on-new-line` hook
                    execute-keys -draft }i J x <a-K> ^<c-r>x(?:end|else|elsif|rescue|when)[^0-9A-Za-z_!?]<ret>
                ]
                execute-keys -draft o<c-r>xend<esc> # insert a new line with containing end
            ]
        ]
    ]
]

define-command -hidden crystal-fetch-keywords %{
    set-register dquote %sh{
        curl --location https://github.com/crystal-lang/crystal/raw/master/src/compiler/crystal/syntax/lexer.cr |
        kak -f '%1scheck_ident_or_keyword\(:(\w+\??), \w+\)<ret>y%<a-R>a<ret><esc><a-_>a<del><esc>|sort<ret>'
    }
}

define-command -hidden crystal-fetch-operators %{
    set-register dquote %sh{
        curl --location https://github.com/crystal-lang/crystal/raw/master/src/compiler/crystal/syntax/parser.cr |
        kak -f '/AtomicWithMethodCheck =<ret>x1s:"([^"]+)"<ret>y%<a-R>i''<esc>a''<ret><esc><a-_>a<del><esc>'
    }
}

define-command -hidden crystal-fetch-objects %{
    set-register dquote %sh{
        curl --location https://crystal-lang.org/api/ |
        # Remove Top Level Namespace
        kak -f '%1sdata-id="github.com/crystal-lang/crystal/(\w+)"<ret>)<a-,>y%<a-R>a<ret><esc><a-_>a<del><esc>'
    }
}

§
