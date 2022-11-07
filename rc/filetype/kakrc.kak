# http://kakoune.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.*/)?(kakrc|.*\.kak) %{
    set-option buffer filetype kak
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=kak %~
    require-module kak

    set-option window static_words %opt{kak_static_words}

    hook window InsertChar \n -group kak-insert kak-insert-on-new-line
    hook window InsertChar \n -group kak-indent kak-indent-on-new-line
    hook window InsertChar [>)}\]] -group kak-indent kak-indent-on-closing-matching
    hook window InsertChar (?![[{(<>)}\]])[^\s\w] -group kak-indent kak-indent-on-closing-char
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange pop:insert:.* -group kak-trim-indent %{ try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d } }
    set-option buffer extra_word_chars '_' '-'

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window kak-.+ }
~

hook -group kak-highlight global WinSetOption filetype=kak %{
    add-highlighter window/kakrc ref kakrc
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/kakrc }
}

provide-module kak %§

require-module sh

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
add-highlighter shared/kakrc/shell5 region -recurse '\{' '(^|\h)\K-?shell-script-(completion|candidates)\h+%\{' '\}' ref sh
add-highlighter shared/kakrc/shell6 region -recurse '\(' '(^|\h)\K-?shell-script-(completion|candidates)\h+%\(' '\)' ref sh
add-highlighter shared/kakrc/shell7 region -recurse '\[' '(^|\h)\K-?shell-script-(completion|candidates)\h+%\[' '\]' ref sh
add-highlighter shared/kakrc/shell8 region -recurse '<'  '(^|\h)\K-?shell-script-(completion|candidates)\h+%<'  '>'  ref sh

evaluate-commands %sh{
    # Grammar
    keywords="add-highlighter alias arrange-buffers buffer buffer-next buffer-previous catch
              change-directory colorscheme debug declare-option declare-user-mode define-command complete-command
              delete-buffer delete-buffer! echo edit edit! enter-user-mode evaluate-commands execute-keys
              fail hook info kill kill! map menu nop on-key prompt provide-module quit quit!
              remove-highlighter remove-hooks rename-buffer rename-client rename-session require-module
              select set-face set-option set-register source trigger-user-hook try
              unalias unmap unset-face unset-option update-option
              write write! write-all write-all-quit write-quit write-quit!"
    attributes="global buffer window current
                normal insert menu prompt goto view user object
                number-lines show-matching show-whitespaces fill regex dynregex group flag-lines
                ranges line column wrap ref regions region default-region replace-ranges"
    types="int bool str regex int-list str-list completions line-specs range-specs str-to-str-map"
    values="default black red green yellow blue magenta cyan white yes no false true"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list kak_static_words $(join "${keywords} ${attributes} ${types} ${values}" ' ')'"

    # Highlight keywords (which are always surrounded by whitespace)
    printf '%s\n' "add-highlighter shared/kakrc/code/keywords regex (?:\s|\A)\K($(join "${keywords}" '|'))(?:(?=\s)|\z) 0:keyword
                   add-highlighter shared/kakrc/code/attributes regex (?:\s|\A)\K($(join "${attributes}" '|'))(?:(?=\s)|\z) 0:attribute
                   add-highlighter shared/kakrc/code/types regex (?:\s|\A)\K($(join "${types}" '|'))(?:(?=\s)|\z) 0:type
                   add-highlighter shared/kakrc/code/values regex (?:\s|\A)\K($(join "${values}" '|'))(?:(?=\s)|\z) 0:value"
}

add-highlighter shared/kakrc/code/colors regex \b(rgb:[0-9a-fA-F]{6}|rgba:[0-9a-fA-F]{8})\b 0:value
add-highlighter shared/kakrc/code/numbers regex \b\d+\b 0:value

add-highlighter shared/kakrc/double_string/fill fill string
add-highlighter shared/kakrc/double_string/escape regex '""' 0:default+b
add-highlighter shared/kakrc/single_string/fill fill string
add-highlighter shared/kakrc/single_string/escape regex "''" 0:default+b

add-highlighter shared/kak ref kakrc

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden kak-insert-on-new-line %~
    evaluate-commands -draft -itersel %=
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*#\h* <ret> y jgh P }
    =
~

define-command -hidden kak-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after line ending with %\w*[^\s\w]
        try %{ execute-keys -draft k x <a-k> \%\w*[^\s\w]$ <ret> j <a-gt> }
        # deindent closing brace when after cursor
        try %_ execute-keys -draft -itersel x <a-k> ^\h*([>)}\]]) <ret> gh / <c-r>1 <ret> m <a-S> 1<a-&> _
        # deindent closing char(s) 
        try %{ execute-keys -draft -itersel x <a-k> ^\h*([^\s\w]) <ret> gh / <c-r>1 <ret> <a-?> <c-r>1 <ret> <a-T>% <a-k> \w*<c-r>1$ <ret> <a-S> 1<a-&> }
    =
~

define-command -hidden kak-indent-on-closing-matching %~
    # align to opening matching brace when alone on a line
    try %= execute-keys -draft -itersel <a-h><a-k>^\h*\Q %val{hook_param} \E$<ret> mGi s \A|.\z<ret> 1<a-&> =
~

define-command -hidden kak-indent-on-closing-char %{
    # align to opening matching character when alone on a line
    try %{ execute-keys -draft -itersel <a-h><a-k>^\h*\Q %val{hook_param} \E$<ret>gi<a-f> %val{hook_param} <a-T>%<a-k>\w*\Q %val{hook_param} \E$<ret> s \A|.\z<ret> gi 1<a-&> }
}

§
