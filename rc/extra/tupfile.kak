# http://gittup.org/tup/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*/?Tup(file|rules)(\.\w+)?$ %{
    set-option buffer filetype tupfile
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code tupfile \
    string '"' (?<!\\)(\\\\)*" '' \
    comment '#' $ ''

add-highlighter shared/tupfile/string fill string
add-highlighter shared/tupfile/comment fill comment

add-highlighter shared/tupfile/code regex "\%[fbBeoOdg]\b" 0:value
add-highlighter shared/tupfile/code regex "[$@]\([\w_]+\)" 0:value
add-highlighter shared/tupfile/code regex "^\h*:\s*(foreach)\b" 1:keyword
add-highlighter shared/tupfile/code regex "^\h*(\.gitignore)\b" 1:keyword
add-highlighter shared/tupfile/code regex "^\h*\b(ifn?eq|ifn?def|else|endif|error|include|include_rules|run|preload|export)\b" 0:keyword
add-highlighter shared/tupfile/code regex "^\h*\b(&?[\w_]+)\s*[:+]?=" 1:keyword
add-highlighter shared/tupfile/code regex "`[^`\n]+`" 0:meta

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group tupfile-highlight global WinSetOption filetype=tupfile %{ add-highlighter window ref tupfile }
hook -group tupfile-highlight global WinSetOption filetype=(?!tupfile).* %{ remove-highlighter window/tupfile }
