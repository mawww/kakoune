# http://gittup.org/tup/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.+/)?[Tt]upfile %{
    set buffer filetype tupfile
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code tupfile \
    string '"' (?<!\\)(\\\\)*" '' \
    comment '#' $ ''

add-highlighter -group /tupfile/string fill string
add-highlighter -group /tupfile/comment fill comment

add-highlighter -group /tupfile/code regex "\%[fbBeoOdg]\b" 0:value
add-highlighter -group /tupfile/code regex "\$\([\w_]+\)" 0:value
add-highlighter -group /tupfile/code regex ":\s*(foreach)\b" 1:keyword
add-highlighter -group /tupfile/code regex "(\.gitignore\b)" 0:keyword
add-highlighter -group /tupfile/code regex "\bifn?eq|ifn?def|else|endif|error|include|include_rules|run|preload|export\b" 0:keyword
add-highlighter -group /tupfile/code regex "\b(\&?[\w_]+)\s*[:+]?=" 1:keyword

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group tupfile-highlight global WinSetOption filetype=tupfile %{ add-highlighter ref tupfile }
hook -group tupfile-highlight global WinSetOption filetype=(?!tupfile).* %{ remove-highlighter tupfile }
