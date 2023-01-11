hook global BufCreate .*\.java %{
    set-option buffer filetype java
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=java %{
    require-module java

    set-option window static_words %opt{java_static_words}

    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange pop:insert:.* -group java-trim-indent %{ try %{ execute-keys -draft xs^\h+$<ret>d } }
    hook window InsertChar \n -group java-insert java-insert-on-new-line
    hook window InsertChar \n -group java-indent java-indent-on-new-line
    hook window InsertChar \{ -group java-indent java-indent-on-opening-curly-brace
    hook window InsertChar \} -group java-indent java-indent-on-closing-curly-brace

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window java-.+ }
}

hook -group java-highlight global WinSetOption filetype=java %{
    add-highlighter window/java ref java
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/java }
}

provide-module java %§

add-highlighter shared/java regions
add-highlighter shared/java/code default-region group
add-highlighter shared/java/string region %{(?<!')"} %{(?<!\\)(\\\\)*"} fill string
add-highlighter shared/java/character region %{'} %{(?<!\\)'} fill value
add-highlighter shared/java/comment region /\* \*/ fill comment
add-highlighter shared/java/inline_documentation region /// $ fill documentation
add-highlighter shared/java/line_comment region // $ fill comment

add-highlighter shared/java/code/ regex "(?<!\w)@\w+\b" 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden java-insert-on-new-line %[
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K/{2,}\h* <ret> y<c-o>P<esc> }
]

define-command -hidden java-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft kx <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
        # indent after a switch's case/default statements
        try %[ execute-keys -draft kx <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after keywords
        try %[ execute-keys -draft <semicolon><a-F>)MB <a-k> \A(if|else|while|for|try|catch)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-,><a-gt> ]
        # deindent closing brace(s) when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
    =
~

define-command -hidden java-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden java-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

# Shell
# ‾‾‾‾‾
# Oracle 2021, 3.9 Keywords, Chapter 3. Lexical Structure, Java Language Specification, Java SE 17, viewed 25 September 2021, <https://docs.oracle.com/javase/specs/jls/se17/html/jls-3.html#jls-3.9>
#
evaluate-commands %sh{
    values='false null this true'

    types='boolean byte char double float int long short unsigned void'

    keywords='assert break case catch class continue default do else enum extends
        finally for if implements import instanceof interface new package return
        static strictfp super switch throw throws try var while yield'

    attributes='abstract final native non-sealed permits private protected public
        record sealed synchronized transient volatile'

    modules='exports module open opens provides requires to transitive uses with'

    # ---------------------------------------------------------------------------------------------- #
    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }
    # ---------------------------------------------------------------------------------------------- #
    add_highlighter() { printf "add-highlighter shared/java/code/ regex %s %s\n" "$1" "$2"; }
    # ---------------------------------------------------------------------------------------------- #
    add_word_highlighter() {

      while [ $# -gt 0 ]; do
          words=$1 face=$2; shift 2
          regex="\\b($(join "${words}" '|'))\\b"
          add_highlighter "$regex" "1:$face"
      done

    }

    # highlight: open<space> not open()
    add_module_highlighter() {

      while [ $# -gt 0 ]; do
          words=$1 face=$2; shift 2
          regex="\\b($(join "${words}" '|'))\\b(?=\\s)"
          add_highlighter "$regex" "1:$face"
      done

    }
    # ---------------------------------------------------------------------------------------------- #
    printf %s\\n "declare-option str-list java_static_words $(join "${values} ${types} ${keywords} ${attributes} ${modules}" ' ')"
    # ---------------------------------------------------------------------------------------------- #
    add_word_highlighter "$values" "value" "$types" "type" "$keywords" "keyword" "$attributes" "attribute"
    # ---------------------------------------------------------------------------------------------- #
    add_module_highlighter "$modules" "module"
    # ---------------------------------------------------------------------------------------------- #
}

§
