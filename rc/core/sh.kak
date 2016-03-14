hook global BufCreate .*\.(z|ba|c|k)?sh(rc|_profile)? %{
    set buffer filetype sh
}

hook global BufSetOption mimetype=text/x-shellscript %{
    set buffer filetype sh
}

addhl -group / regions -default code sh \
    double_string  %{(?<!\\)(\\\\)*\K"} %{(?<!\\)(\\\\)*"} '' \
    single_string %{(?<!\\)(\\\\)*\K'} %{'} '' \
    comment '(?<!\$)#' '$' ''

addhl -group /sh/double_string fill string
addhl -group /sh/single_string fill string
addhl -group /sh/comment fill comment

%sh{
    # Grammar
    keywords="alias:bind:builtin:caller:case:cd:command:coproc:declare:do:done"
    keywords="${keywords}:echo:elif:else:enable:esac:exit:fi:for:function:help"
    keywords="${keywords}:if:in:let:local:logout:mapfile:printf:read:readarray"
    keywords="${keywords}:readonly:return:select:set:shift:source:test:then"
    keywords="${keywords}:time:type:typeset:ulimit:unalias:until:while"

    # Add the language's grammar to the static completion list
    echo "hook global WinSetOption filetype=sh %{
        set window static_words '${keywords}'
    }"

    # Highlight keywords
    echo "
        addhl -group /sh/code regex \<(${keywords//:/|})\> 0:keyword
    "
}

addhl -group /sh/code regex [\[\]\(\)&|]{2}|\[\s|\s\] 0:operator
addhl -group /sh/code regex (\w+)= 1:identifier
addhl -group /sh/code regex ^\h*(\w+)\h*\(\) 1:identifier

addhl -group /sh/code regex \$(\w+|\{.+?\}|#|@|\?|\$|!|-|\*) 0:identifier
addhl -group /sh/double_string regex \$(\w+|\{.+?\}) 0:identifier

hook global WinSetOption filetype=sh %{ addhl ref sh }
hook global WinSetOption filetype=(?!sh).* %{ rmhl sh }
