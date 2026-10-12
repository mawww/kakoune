# https://vala.dev
#
# Vala Reference Manual, Types and Preprocessor, viewed 1 October 2026, https://gnome.pages.gitlab.gnome.org/vala/manual/
# Vala 0.56 scanner (keywords, literals and escape sequences), https://gitlab.gnome.org/GNOME/vala/-/blob/0.56/vala/valascanner.vala

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](vala|vapi) %{
    set-option buffer filetype vala
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=vala %{
    require-module vala

    set-option window static_words %opt{vala_static_words}

    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange pop:insert:.* -group vala-trim-indent c-family-trim-indent
    hook window InsertChar \n -group vala-insert c-family-insert-on-newline
    hook window InsertChar \n -group vala-indent c-family-indent-on-newline
    hook window InsertChar \n -group vala-indent vala-indent-on-new-line
    hook window InsertChar \{ -group vala-indent c-family-indent-on-opening-curly-brace
    hook window InsertChar \} -group vala-indent c-family-indent-on-closing-curly-brace

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window vala-.+ }
}

hook -group vala-highlight global WinSetOption filetype=vala %{
    add-highlighter window/vala ref vala
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/vala }
}

provide-module vala %§

require-module c-family

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/vala regions
add-highlighter shared/vala/code default-region group
add-highlighter shared/vala/verbatim_string region '"""' '"""' fill string
add-highlighter shared/vala/template_string region '@"' %{(?<!\\)(\\\\)*"} group
add-highlighter shared/vala/string region %{(?<!')"} %{(?<!\\)(\\\\)*"} group
add-highlighter shared/vala/character region %{'} %{(?<!\\)(\\\\)*'} fill value
add-highlighter shared/vala/documentation region /\*\*(?!/) \*/ fill documentation
add-highlighter shared/vala/comment region /\* \*/ fill comment
add-highlighter shared/vala/line_comment region // $ fill comment
add-highlighter shared/vala/regex region (?<=[=(,:!&|?])\h*/(?![/*]) (?<!\\)(\\\\)*/[imsx]* fill meta
add-highlighter shared/vala/preprocessor region ^\h*# $ fill meta

add-highlighter shared/vala/string/ fill string
add-highlighter shared/vala/string/ regex \\(?:[\\'"0bfnrtv$]|x[0-9a-fA-F]+|u[0-9a-fA-F]{4}) 0:meta

add-highlighter shared/vala/template_string/ fill string
add-highlighter shared/vala/template_string/ regex \\(?:[\\'"0bfnrtv$]|x[0-9a-fA-F]+|u[0-9a-fA-F]{4}) 0:meta
add-highlighter shared/vala/template_string/ regex \$(?:\w+|\([^)]*\)) 0:value

add-highlighter shared/vala/code/ regex \b([a-zA-Z_]\w*)\h*(?=\() 1:function
add-highlighter shared/vala/code/ regex \b[A-Z]\w*[a-z]\w*\b 0:type
add-highlighter shared/vala/code/ regex (?<![\w.])(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)(?:[lL]{1,2}|[uU][lL]{0,2}|[fFdD])?\b 0:value
add-highlighter shared/vala/code/ regex ^\h*\[\h*\K[A-Z]\w* 0:meta
add-highlighter shared/vala/code/ regex \bglobal(?=::) 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

# c-family handles if, for and while; this adds foreach
define-command -hidden vala-indent-on-new-line %< evaluate-commands -draft -itersel %<
    execute-keys <semicolon>
    # indent after a foreach not followed by an opening brace
    try %< execute-keys -draft k x s\)\h*(?://\N+)?\n\z<ret> \
                               <a-semicolon>mB <a-k>\A\bforeach\b<ret> <a-semicolon>j <a-gt> >
    # deindent after its single line statement
    try %< execute-keys -draft K x <a-k>\;\h*(//\N+)?$<ret> \
                               K x s\)(\h+\w+)*\h*(//\N+)?\n(\N*\n){2}\z<ret> \
                               MB <a-k>\A\bforeach\b<ret> <a-S>1<a-&> >
> >

# Shell
# ‾‾‾‾‾

evaluate-commands %sh{
    values='true false null this base'

    # fundamental types from the reference manual, plus string, void and var
    types='bool char uchar short ushort int uint long ulong size_t ssize_t int8
        uint8 int16 uint16 int32 uint32 int64 uint64 unichar float double string
        void var'

    keywords='if else switch case default do while for foreach in break continue
        return try catch finally throw throws lock unlock yield new delete sizeof
        typeof is as out ref get set construct using namespace class struct
        interface enum errordomain delegate signal with requires ensures params'

    attributes='abstract const dynamic extern inline internal override owned
        partial private protected public sealed static unowned virtual volatile
        weak async'

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    printf %s\\n "declare-option str-list vala_static_words $(join "${values} ${types} ${keywords} ${attributes}" ' ')"

    # skip @-escaped identifiers and member accesses such as map.set ()
    for pair in "values:value" "types:type" "keywords:keyword" "attributes:attribute"; do
        var=${pair%%:*} face=${pair##*:}
        eval words=\$$var
        printf 'add-highlighter shared/vala/code/ regex %s %s\n' "(?<![@.\\w])($(join "${words}" '|'))\\b" "1:$face"
    done
}

§
