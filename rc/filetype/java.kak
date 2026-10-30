hook global BufCreate .*\.java %{
    set-option buffer filetype java
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=java %[
    require-module c-family
    require-module java

    set-option window static_words %opt{java_static_words}

    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange pop:insert:.* -group java-trim-indent c-family-trim-indent
    hook window InsertChar \n -group java-insert c-family-insert-on-newline
    hook window InsertChar \n -group java-indent c-family-indent-on-newline
    hook window InsertChar \{ -group java-indent c-family-indent-on-opening-curly-brace
    hook window InsertChar \} -group java-indent c-family-indent-on-closing-curly-brace
    hook window InsertChar \} -group java-insert c-family-insert-on-closing-curly-brace

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window java-.+ }
]

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
#
# Shell
# ‾‾‾‾‾
# Oracle 2026, 3.9 Keywords, Chapter 3. Lexical Structure, Java Language Specification, Java SE 26, viewed 12 October 2026, <https://docs.oracle.com/en/java/javase/26/docs/specs/jls/jls-3.html#jls-3.9>
#
evaluate-commands %sh{
    values='false null this true'

    types='boolean byte char double float int long short void'

    keywords='assert break case catch class continue default do else enum extends
        finally for if implements import instanceof interface new package return
        static strictfp super switch throw throws try var when while yield'

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
