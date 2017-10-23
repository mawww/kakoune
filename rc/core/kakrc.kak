# http://kakoune.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    set buffer filetype kak
}

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code kakrc \
    comment (^|\h)\K\# $ '' \
    double_string %{(^|\h)"} %{(?<!\\)(\\\\)*"} '' \
    single_string %{(^|\h)'} %{(?<!\\)(\\\\)*'} '' \
    shell '%sh\{' '\}' '\{' \
    shell '%sh\(' '\)' '\(' \
    shell '%sh\[' '\]' '\[' \
    shell '%sh\<' '\>' '\<' \
    shell '-shell-(completion|candidates)\h+%\{' '\}' '\{' \
    shell '-shell-(completion|candidates)\h+%\(' '\)' '\(' \
    shell '-shell-(completion|candidates)\h+%\[' '\]' '\[' \
    shell '-shell-(completion|candidates)\h+%\<' '\>' '\<'

%sh{
    # Grammar
    keywords="edit write write-all kill quit write-quit write-all-quit map unmap alias unalias
              buffer buffer-next buffer-previous delete-buffer add-highlighter remove-highlighter
              hook remove-hooks define-command echo debug source try fail
              set-option unset-option update-option declare-option exec eval prompt menu on-key info
              set-face rename-client set-register select change-directory rename-session colorscheme"
    attributes="global buffer window current
                normal insert menu prompt goto view user object
                number_lines show_matching show_whitespaces fill regex dynregex group flag_lines
                ranges line column wrap ref regions replace-ranges"
    types="int bool str regex int-list str-list completions line-specs range-specs"
    values="default black red green yellow blue magenta cyan white"

    join() { printf "%s" "$1" | tr -s ' \n' "$2"; }

    # Add the language's grammar to the static completion list
    printf '%s\n' "hook global WinSetOption filetype=kak %{
        set window static_words '$(join "${keywords}:${attributes}:${types}:${values}" '|')'
        set -- window extra_word_chars '-'
    }"

    # Highlight keywords (which are always surrounded by whitespace)
    printf '%s\n' "add-highlighter -group /kakrc/code regex [\s\A]\K($(join "${keywords}" '|'))(?=[\s\z])\b 0:keyword
                   add-highlighter -group /kakrc/code regex [\s\A]\K($(join "${attributes}" '|'))(?=[\s\z])\b 0:attribute
                   add-highlighter -group /kakrc/code regex [\s\A]\K($(join "${types}" '|'))(?=[\s\z])\b 0:type
                   add-highlighter -group /kakrc/code regex [\s\A]\K($(join "${values}" '|'))(?=[\s\z])\b 0:value"
}

add-highlighter -group /kakrc/code regex \brgb:[0-9a-fA-F]{6}\b 0:value

add-highlighter -group /kakrc/double_string fill string
add-highlighter -group /kakrc/single_string fill string
add-highlighter -group /kakrc/comment fill comment
add-highlighter -group /kakrc/shell ref sh

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden kak-indent-on-new-line %{
    eval -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*#\h* <ret> y jgh P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ exec -draft k <a-x> s \h+$ <ret> d }
        # indent after line ending with %[[:punct:]]
        try %{ exec -draft k <a-x> <a-k> \%[[:punct:]]$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group kak-highlight global WinSetOption filetype=kak %{ add-highlighter ref kakrc }

hook global WinSetOption filetype=kak %{
    hook window InsertChar \n -group kak-indent kak-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window InsertEnd .* -group kak-indent %{ try %{ exec -draft \; <a-x> s ^\h+$ <ret> d } }
}

hook -group kak-highlight global WinSetOption filetype=(?!kak).* %{ remove-highlighter kakrc }
hook global WinSetOption filetype=(?!kak).* %{ remove-hooks window kak-indent }
