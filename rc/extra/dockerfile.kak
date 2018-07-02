# http://docker.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# See https://docs.docker.com/reference/builder

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*/?Dockerfile(\.\w+)?$ %{
    set-option buffer filetype dockerfile
}

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
    printf %s\\n "hook global WinSetOption filetype=dockerfile %{
        set window static_words ONBUILD|${keywords}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/dockerfile/code/ regex '^(?i)(ONBUILD\h+)?(${keywords})\b' 2:keyword
        add-highlighter shared/dockerfile/code/ regex '^(?i)(ONBUILD)\h+' 1:keyword
    "
}

add-highlighter shared/dockerfile/code/ regex '\$\{[\w_]+\}' 0:value
add-highlighter shared/dockerfile/code/ regex '\$[\w_]+' 0:value

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group dockerfile-highlight global WinSetOption filetype=dockerfile %{ add-highlighter window/dockerfile ref dockerfile }
hook -group dockerfile-highlight global WinSetOption filetype=(?!dockerfile).* %{ remove-highlighter window/dockerfile }
