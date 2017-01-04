# http://docker.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# See https://docs.docker.com/reference/builder

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*Dockerfile %{
    set buffer filetype dockerfile
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions dockerfile                                                                                             \
    instruction '^(?i)(ONBUILD\h+)?(FROM|MAINTAINER|RUN|CMD|LABEL|EXPOSE|ENV|ADD|COPY|ENTRYPOINT|VOLUME|USER|WORKDIR)' '$' '' \
    comment     '#'                                                                                                    '$' ''

add-highlighter -group /dockerfile/instruction regex '^(?i)(ONBUILD\h+)?(FROM|MAINTAINER|RUN|CMD|LABEL|EXPOSE|ENV|ADD|COPY|ENTRYPOINT|VOLUME|USER|WORKDIR)' 0:keyword

add-highlighter -group /dockerfile/instruction regions regions                           \
    plain '^(?i)(ONBUILD\h+)?(LABEL|ENV)'                               '$' '' \
    json  '^(?i)(ONBUILD\h+)?(RUN|CMD|ADD|COPY|ENTRYPOINT|VOLUME)\h+\[' \]  \[ \
    sh    '^(?i)(ONBUILD\h+)?(RUN|CMD|ENTRYPOINT)\h+([A-Z/a-z])+'       '$' ''

add-highlighter -group /dockerfile/instruction/regions/plain regions regions \
    string '"' '(?<!\\)(\\\\)*"' ''                                \
    string "'" "'"               ''

add-highlighter -group /dockerfile/instruction/regions/plain/regions/string fill string

add-highlighter -group /dockerfile/instruction/regions/json ref json
add-highlighter -group /dockerfile/instruction/regions/sh   ref sh

add-highlighter -group /dockerfile/comment fill comment

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group dockerfile-highlight global WinSetOption filetype=dockerfile %{ add-highlighter ref dockerfile }
hook -group dockerfile-highlight global WinSetOption filetype=(?!dockerfile).* %{ remove-highlighter dockerfile }
