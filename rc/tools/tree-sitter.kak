declare-option str-list tree_sitter_grammar_directories \
    "%val{runtime}/grammars"                            \
    "%val{config}/grammars"                             \

declare-option str-list tree_sitter_default_faces \
    text.literal:comment                          \
    text.reference:variable                       \
    text.title:title                              \
    text.uri:+u                                   \
    text.underline:+u                             \
    text.todo:meta                                \
    comment:comment                               \
    punctuation.special:meta                      \
                                                  \
    constant.builtin:keyword                      \
    constant.macro:meta                           \
    define:meta                                   \
    macro:meta                                    \
    string:string                                 \
    string.escape:operator                        \
    string.special:operator                       \
    character:string                              \
    character.special:operator                    \
    number:value                                  \
    boolean:value                                 \
    float:value                                   \
                                                  \
    keyword:keyword                               \
    function:function                             \
    function.builtin:builtin                      \
    function.macro:meta                           \
    parameter:variable                            \
    method:function                               \
    field:variable                                \
    property:variable                             \
    constructor:function                          \
                                                  \
    variable:variable                             \
    type:type                                     \
    type.definition:type                          \
    storageclass:type                             \
    structure:type                                \
    namespace:type                                \
    include:type                                  \
    preproc:meta                                  \
    debug:error                                   \
    tag:attribute                                 \

provide-module tree-sitter %{
    define-command tree-sitter-load-highlighter -params 2.. -docstring %{
    } %{ try %{ evaluate-commands %sh{
        path="$1"
        lang="$2"
        shift 2
        faces="$*"

        escape() {
            printf "'%s'" $(printf "%s" "$1" | sed "s|'|''|g")
        }

        eval set -- "$kak_quoted_opt_tree_sitter_grammar_directories"
        for directory in "$@" ; do
            grammar="$directory/$lang"
            [ -e "$grammar" ] || continue
            printf "%s\n" \
                "add-highlighter $path tree-sitter $lang $(escape "$grammar") $faces"
        done
    }}}
}
