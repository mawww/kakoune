# http://gittup.org/tup/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.+/)?[Tt]upfile %{
    set buffer mimetype ""
    set buffer filetype tupfile
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code tupfile \
    string '"' (?<!\\)(\\\\)*" '' \
    comment '#' $ ''

addhl -group /tupfile/string fill string
addhl -group /tupfile/comment fill comment

addhl -group /tupfile/code regex "\%[fbBeoOdg]\b" 0:value
addhl -group /tupfile/code regex "\$\([\w_]+\)" 0:value
addhl -group /tupfile/code regex ":\s*(foreach)\b" 1:keyword
addhl -group /tupfile/code regex "(\.gitignore\b)" 0:keyword
addhl -group /tupfile/code regex "\bifn?eq|ifn?def|else|endif|error|include|include_rules|run|preload|export\b" 0:keyword
addhl -group /tupfile/code regex "\b(\&?[\w_]+)\s*[:+]?=" 1:keyword

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=tupfile %{
    addhl ref tupfile
}

hook global WinSetOption filetype=(?!tupfile).* %{
    rmhl tupfile
}
