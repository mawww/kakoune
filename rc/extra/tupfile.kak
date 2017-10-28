# http://gittup.org/tup/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.+/)?[Tt]upfile %{
    set buffer filetype tupfile
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code tupfile \
    string '"' (?<!\\)(\\\\)*" '' \
    comment '#' $ ''

add-highlighter shared/tupfile/string fill string
add-highlighter shared/tupfile/comment fill comment

add-highlighter shared/tupfile/code regex "\%[fbBeoOdg]\b" 0:value
add-highlighter shared/tupfile/code regex "\$\([\w_]+\)" 0:value
add-highlighter shared/tupfile/code regex ":\s*(foreach)\b" 1:keyword
add-highlighter shared/tupfile/code regex "\.gitignore\b" 0:keyword
add-highlighter shared/tupfile/code regex "\b(ifn?eq|ifn?def|else|endif|error|include|include_rules|run|preload|export)\b" 0:keyword
add-highlighter shared/tupfile/code regex "\b(&?[\w_]+)\s*[:+]?=" 1:keyword

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group tupfile-highlight global WinSetOption filetype=tupfile %{ add-highlighter window ref tupfile }
hook -group tupfile-highlight global WinSetOption filetype=(?!tupfile).* %{ remove-highlighter window/tupfile }
