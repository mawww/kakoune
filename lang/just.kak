# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*/?[jJ]ustfile %{
    set-option buffer filetype justfile
}

# Indentation
# ‾‾‾‾‾‾‾‾‾‾‾

define-command -hidden just-indent-on-new-line %{
     evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \;K<a-&> }
        # cleanup trailing white spaces on previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>"_d }
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*//\h* <ret> y jgh P }
    }
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/justfile regions
add-highlighter shared/justfile/content default-region group
add-highlighter shared/justfile/comment  region '#' '$'  fill comment
add-highlighter shared/justfile/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/justfile/single_string region "'" (?<!\\)(\\\\)*' fill string
add-highlighter shared/justfile/inline   region '`' '`' ref sh
add-highlighter shared/justfile/shell    region '^\h+' '^[^\h]' group

add-highlighter shared/justfile/shell/ ref sh

add-highlighter shared/justfile/content/ regex '^(@)?([\w-]+)(?:\s(.+))?\s?(:)(.+)?$' 1:operator 2:function 3:value 4:operator 5:type
add-highlighter shared/justfile/content/ regex '([=+])' 1:operator
add-highlighter shared/justfile/content/ regex '^([\w-]+)\s=' 1:value

hook global WinSetOption filetype=justfile %{
    hook window InsertChar \n -group justfile-indent just-indent-on-new-line
    add-highlighter window/justfile ref justfile

    # variable substititution highlighting
    add-highlighter shared/justfile/shell regex '(\{{2})([\w-]+)(\}{2})' 1:operator 2:variable 3:operator

}

hook global WinSetOption filetype=(?!justfile).* %{
    remove-highlighter window/justfile
    remove-hooks window justfile-indent
}

