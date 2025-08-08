# https://prql-lang.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module detect-prql %{

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](prql) %{
    set-option buffer filetype prql
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=prql %{
    require-module prql

    set-option window static_words %opt{prql_static_words}

    hook window InsertChar \n -group prql-insert prql-insert-on-new-line
    hook window InsertChar \n -group prql-indent prql-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange pop:insert:.* -group prql-trim-indent %{ try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d } }
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window prql-.+ }
}

hook -group prql-highlight global WinSetOption filetype=prql %{
    add-highlighter window/prql ref prql
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/prql }
}

}

require-module detect-prql

provide-module prql %§

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/prql regions
add-highlighter shared/prql/code default-region group

add-highlighter shared/prql/documentation region '##'  '$'              fill documentation
add-highlighter shared/prql/comment       region '#'   '$'              fill comment

# String interpolation
add-highlighter shared/prql/f_triple_string region -match-capture f("""|''') (?<!\\)(?:\\\\)*("""|''') group
add-highlighter shared/prql/f_triple_string/ fill string
add-highlighter shared/prql/f_triple_string/ regex \{.*?\} 0:value

add-highlighter shared/prql/f_double_string region 'f"'   (?<!\\)(\\\\)*" group
add-highlighter shared/prql/f_double_string/ fill string
add-highlighter shared/prql/f_double_string/ regex \{.*?\} 0:value

add-highlighter shared/prql/f_single_string region "f'"   (?<!\\)(\\\\)*' group
add-highlighter shared/prql/f_single_string/ fill string
add-highlighter shared/prql/f_single_string/ regex \{.*?\} 0:value


# Regular string
add-highlighter shared/prql/triple_string region -match-capture ("""|''') (?<!\\)(?:\\\\)*("""|''') fill string
add-highlighter shared/prql/double_string region '"'   (?<!\\)(\\\\)*" fill string
add-highlighter shared/prql/single_string region "'"   (?<!\\)(\\\\)*' fill string

# Integer formats
add-highlighter shared/prql/code/ regex '(?i)\b0b[01]+l?\b' 0:value
add-highlighter shared/prql/code/ regex '(?i)\b0x[\da-f]+l?\b' 0:value
add-highlighter shared/prql/code/ regex '(?i)\b0o?[0-7]+l?\b' 0:value
add-highlighter shared/prql/code/ regex '(?i)\b([1-9]\d*|0)l?\b' 0:value
# Float formats
add-highlighter shared/prql/code/ regex '\b\d+[eE][+-]?\d+\b' 0:value
add-highlighter shared/prql/code/ regex '(\b\d+)?\.\d+\b' 0:value
add-highlighter shared/prql/code/ regex '\b\d+\.' 0:value

evaluate-commands %sh{
    # Grammar
    values="true false null this that"
    meta="prql module"

    keywords="case let type alias in loop"

    types="bool float int int8 int16 int32 int64 int128 text date time timestamp"

    functions="aggregate derive filter from group join select sort take window"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list prql_static_words $(join "${values} ${meta} ${keywords} ${types} ${functions}" ' ')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/prql/code/ regex '\b($(join "${values}" '|'))\b' 0:value
        add-highlighter shared/prql/code/ regex '\b($(join "${meta}" '|'))\b' 0:meta
        add-highlighter shared/prql/code/ regex '\b($(join "${keywords}" '|'))\b' 0:keyword
        add-highlighter shared/prql/code/ regex '\b($(join "${functions}" '|'))\b\(' 1:builtin
        add-highlighter shared/prql/code/ regex '\b($(join "${types}" '|'))\b' 0:type
        add-highlighter shared/prql/code/ regex '^\h*(@\{[\w_.]+\}))' 1:attribute
    "
}

add-highlighter shared/prql/code/ regex (?<=[\w\s\d\)\]'"_])(<=|>=|<>?|>|!=|==|~=|\||\^|&|\+|-|\*\*?|//?|%|~) 0:operator
add-highlighter shared/prql/code/ regex (?<=[\w\s\d'"_])((?<![=<>!]):?=(?![=])|[+*-]=) 0:builtin
add-highlighter shared/prql/code/ regex ^\h*(?:module)\h+(\S+) 1:module

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden prql-insert-on-new-line %{ evaluate-commands -itersel -draft %{
    execute-keys <semicolon>
    try %{
        evaluate-commands -draft -save-regs '/"' %{
            # Ensure previous line is a comment
            execute-keys -draft kxs^\h*#+\h*<ret>

            # now handle the comment continuation logic
            try %{
                # try and match a regular block comment, copying the prefix
                execute-keys -draft -save-regs '' k x 1s^(\h*#+\h*)\S.*$ <ret> y
                execute-keys -draft P
            } catch %{
                try %{
                    # try and match a regular block comment followed by a single
                    # empty comment line
                    execute-keys -draft -save-regs '' kKx 1s^(\h*#+\h*)\S+\n\h*#+\h*$ <ret> y
                    execute-keys -draft P
                } catch %{
                    try %{
                        # try and match a pair of empty comment lines, and delete
                        # them if we match
                        execute-keys -draft kKx <a-k> ^\h*#+\h*\n\h*#+\h*$ <ret> <a-d>
                    } catch %{
                        # finally, we need a special case for a new line inserted
                        # into a file that consists of a single empty comment - in
                        # that case we can't expect to copy the trailing whitespace,
                        # so we add our own
                        execute-keys -draft -save-regs '' k x1s^(\h*#+)\h*$<ret> y
                        execute-keys -draft P
                        execute-keys -draft i<space>
                    }
                }
            }
        }

        # trim trailing whitespace on the previous line
        try %{ execute-keys -draft k x s\h+$<ret> d }
    }
} }

define-command -hidden prql-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after line ending with :
        try %{ execute-keys -draft , k x <a-k> :$ <ret> <a-K> ^\h*# <ret> j <a-gt> }
        # deindent closing brace/bracket when after cursor (for arrays and dictionaries)
        try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

§
