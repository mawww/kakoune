# http://kakoune.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    set-option buffer filetype kak
}

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/kakrc regions
add-highlighter shared/kakrc/code default-region group
add-highlighter shared/kakrc/comment region (^|\h)\K# $ fill comment
add-highlighter shared/kakrc/double_string region -recurse %{(?<!")("")+(?!")} %{(^|\h)\K"} %{"(?!")} group
add-highlighter shared/kakrc/single_string region -recurse %{(?<!')('')+(?!')} %{(^|\h)\K'} %{'(?!')} group
add-highlighter shared/kakrc/shell1 region -recurse '\{' '(^|\h)\K%?%sh\{' '\}' ref sh
add-highlighter shared/kakrc/shell2 region -recurse '\(' '(^|\h)\K%?%sh\(' '\)' ref sh
add-highlighter shared/kakrc/shell3 region -recurse '\[' '(^|\h)\K%?%sh\[' '\]' ref sh
add-highlighter shared/kakrc/shell4 region -recurse '<'  '(^|\h)\K%?%sh<'  '>'  ref sh
add-highlighter shared/kakrc/shell5 region -recurse '\{' '(^|\h)\K-shell-(completion|candidates)\h+%\{' '\}' ref sh
add-highlighter shared/kakrc/shell6 region -recurse '\(' '(^|\h)\K-shell-(completion|candidates)\h+%\(' '\)' ref sh
add-highlighter shared/kakrc/shell7 region -recurse '\[' '(^|\h)\K-shell-(completion|candidates)\h+%\[' '\]' ref sh
add-highlighter shared/kakrc/shell8 region -recurse '<'  '(^|\h)\K-shell-(completion|candidates)\h+%<'  '>'  ref sh

evaluate-commands %sh{
    # Grammar
    keywords="edit write write-all kill quit write-quit write-all-quit map unmap alias unalias
              buffer buffer-next buffer-previous delete-buffer add-highlighter remove-highlighter
              hook remove-hooks define-command echo debug source try fail nop
              set-option unset-option update-option declare-option execute-keys evaluate-commands
              prompt menu on-key info set-face unset-face rename-client set-register select
              change-directory rename-session colorscheme declare-user-mode enter-user-mode"
    attributes="global buffer window current
                normal insert menu prompt goto view user object
                number-lines show-matching show-whitespaces fill regex dynregex group flag-lines
                ranges line column wrap ref regions region default-region replace-ranges"
    types="int bool str regex int-list str-list completions line-specs range-specs"
    values="default black red green yellow blue magenta cyan white yes no false true"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf '%s\n' "hook global WinSetOption filetype=kak %{
        set-option window static_words $(join "${keywords} ${attributes} ${types} ${values}" ' ')'
        set-option -- window extra_word_chars '-'
    }"

    # Highlight keywords (which are always surrounded by whitespace)
    printf '%s\n' "add-highlighter shared/kakrc/code/keywords regex (?:\s|\A)\K($(join "${keywords}" '|'))(?:(?=\s)|\z) 0:keyword
                   add-highlighter shared/kakrc/code/attributes regex (?:\s|\A)\K($(join "${attributes}" '|'))(?:(?=\s)|\z) 0:attribute
                   add-highlighter shared/kakrc/code/types regex (?:\s|\A)\K($(join "${types}" '|'))(?:(?=\s)|\z) 0:type
                   add-highlighter shared/kakrc/code/values regex (?:\s|\A)\K($(join "${values}" '|'))(?:(?=\s)|\z) 0:value"
}

add-highlighter shared/kakrc/code/colors regex \brgb:[0-9a-fA-F]{6}\b 0:value
add-highlighter shared/kakrc/code/scopes regex \b(global|shared|buffer|window)(?:\b|/) 0:value

add-highlighter shared/kakrc/double_string/fill fill string
add-highlighter shared/kakrc/double_string/escape regex '""' 0:default+b
add-highlighter shared/kakrc/single_string/fill fill string
add-highlighter shared/kakrc/single_string/escape regex "''" 0:default+b

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden kak-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*#\h* <ret> y jgh P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k <a-x> s \h+$ <ret> d }
        # indent after line ending with %[\W\S]
        try %{ execute-keys -draft k <a-x> <a-k> \%[\W\S]$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group kak-highlight global WinSetOption filetype=kak %{ add-highlighter window/kakrc ref kakrc }

hook global WinSetOption filetype=kak %{
    hook window InsertChar \n -group kak-indent kak-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange insert:.* -group kak-indent %{ try %{ execute-keys -draft \; <a-x> s ^\h+$ <ret> d } }
    set-option buffer extra_word_chars '-'
}

hook -group kak-highlight global WinSetOption filetype=(?!kak).* %{ remove-highlighter window/kakrc }
hook global WinSetOption filetype=(?!kak).* %{ remove-hooks window kak-indent }
