# http://docker.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# See https://docs.docker.com/reference/builder

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*/?Dockerfile(\..+)?$ %{
    set-option buffer filetype dockerfile
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=dockerfile %{
    require-module dockerfile
    set-option window static_words %opt{dockerfile_static_words}
}

hook -group dockerfile-highlight global WinSetOption filetype=dockerfile %{
    add-highlighter window/dockerfile ref dockerfile
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/dockerfile }
}

provide-module dockerfile %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/dockerfile regions
add-highlighter shared/dockerfile/code default-region group
add-highlighter shared/dockerfile/double_string region '"' '(?<!\\)(\\\\)*"' fill string
add-highlighter shared/dockerfile/single_string region "'" "'"               fill string
add-highlighter shared/dockerfile/comment region '#' $ fill comment

evaluate-commands %sh{
    # Grammar
    keywords="ADD|ARG|CMD|COPY|ENTRYPOINT|ENV|EXPOSE|FROM|HEALTHCHECK|LABEL"
    keywords="${keywords}|MAINTAINER|RUN|SHELL|STOPSIGNAL|USER|VOLUME|WORKDIR"

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list dockerfile_static_words ONBUILD|${keywords}" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/dockerfile/code/ regex '^(?i)(ONBUILD\h+)?(${keywords})\b' 2:keyword
        add-highlighter shared/dockerfile/code/ regex '^(?i)(ONBUILD)\h+' 1:keyword
    "
}

add-highlighter shared/dockerfile/code/ regex (?<!\\)(?:\\\\)*\K\$\{[\w_]+\} 0:value
add-highlighter shared/dockerfile/code/ regex (?<!\\)(?:\\\\)*\K\$[\w_]+ 0:value

}
