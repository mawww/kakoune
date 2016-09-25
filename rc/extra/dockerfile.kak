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

addhl -group / regions dockerfile                                                                                             \
    instruction '^(?i)(ONBUILD\h+)?(FROM|MAINTAINER|RUN|CMD|LABEL|EXPOSE|ENV|ADD|COPY|ENTRYPOINT|VOLUME|USER|WORKDIR)' '$' '' \
    comment     '#'                                                                                                    '$' ''

addhl -group /dockerfile/instruction regex '^(?i)(ONBUILD\h+)?(FROM|MAINTAINER|RUN|CMD|LABEL|EXPOSE|ENV|ADD|COPY|ENTRYPOINT|VOLUME|USER|WORKDIR)' 0:keyword

addhl -group /dockerfile/instruction regions regions                           \
    plain '^(?i)(ONBUILD\h+)?(LABEL|ENV)'                               '$' '' \
    json  '^(?i)(ONBUILD\h+)?(RUN|CMD|ADD|COPY|ENTRYPOINT|VOLUME)\h+\[' \]  \[ \
    sh    '^(?i)(ONBUILD\h+)?(RUN|CMD|ENTRYPOINT)\h+([A-Z/a-z])+'       '$' ''

addhl -group /dockerfile/instruction/regions/plain regions regions \
    string '"' '(?<!\\)(\\\\)*"' ''                                \
    string "'" "'"               ''

addhl -group /dockerfile/instruction/regions/plain/regions/string fill string

addhl -group /dockerfile/instruction/regions/json ref json
addhl -group /dockerfile/instruction/regions/sh   ref sh

addhl -group /dockerfile/comment fill comment

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group dockerfile-highlight global WinSetOption filetype=dockerfile %{ addhl ref dockerfile }
hook global WinSetOption filetype=(?!dockerfile).* %{ rmhl dockerfile }
