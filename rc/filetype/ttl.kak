# Terse RDF Triple Language (Turtle)
# of the W3C's Resource Description Framework (RDF):
# https://www.w3.org/TR/turtle/

provide-module detect-ttl %{

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(ttl) %{
    set-option buffer filetype ttl
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ttl %{
    require-module ttl

    hook window ModeChange pop:insert:.* -group ttl-trim-indent ttl-trim-indent
    hook window InsertChar \n -group ttl-insert ttl-insert-on-new-line
    hook window InsertChar \n -group ttl-indent ttl-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window ttl-.+ }
}

hook -group ttl-highlight global WinSetOption filetype=ttl %{
    add-highlighter window/ttl ref ttl
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ttl }
}

}

require-module detect-ttl


provide-module ttl %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ttl regions
add-highlighter shared/ttl/code default-region group

add-highlighter shared/ttl/comment region '#+\h'   $           fill comment
add-highlighter shared/ttl/string1 region  '"""' (?<!\\)(\\\\)*"""(?!") fill string
add-highlighter shared/ttl/string2 region  "'''" "'''(?!')"             fill string
add-highlighter shared/ttl/string3 region  '"'   (?<!\\)(\\\\)*"        fill string
add-highlighter shared/ttl/string4 region  "'"   "'"                    fill string

add-highlighter shared/ttl/code/comment2  regex '^\h*(#+)$'                1:comment
add-highlighter shared/ttl/code/colon     regex (:)                        1:operator
add-highlighter shared/ttl/code/separator regex ([\;\.,<>\[\]\(\)])        1:delimiter
add-highlighter shared/ttl/code/term      regex :(w+)\b                    1:variable
add-highlighter shared/ttl/code/prefix    regex \b([-.\w]+):               1:module
add-highlighter shared/ttl/code/def       regex (@\w+)\b                   1:attribute
add-highlighter shared/ttl/code/cdef      regex \b(BASE|PREFIX)\b          1:attribute
add-highlighter shared/ttl/code/type      regex (\^\^)                     1:type
add-highlighter shared/ttl/code/bool      regex \b(true|false)\b           1:value
add-highlighter shared/ttl/code/num       regex \b([+-]?[0-9]+\.*[0-9]*)\b 1:value
add-highlighter shared/ttl/code/keyword   regex \b(_|a)\b                  1:keyword
# Last position, to override single characters.
add-highlighter shared/ttl/code/IRI       regex <(\H*)>                    1:identifier


# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden ttl-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden ttl-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy # comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K#\h* <ret> y gh j P }
    }
}

define-command -hidden ttl-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : ttl-trim-indent <ret> }
    }
}

}
