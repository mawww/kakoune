# http://editorconfig.org/#file-format-details
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

declare-option -hidden bool editorconfig_trim_trailing_whitespace false

define-command editorconfig-load -params ..1 -docstring "editorconfig-load [file]: set formatting behavior according to editorconfig" %{
    remove-hooks buffer editorconfig-hooks
    evaluate-commands %sh{
        command -v editorconfig >/dev/null 2>&1 || { echo 'echo -markup "{Error}editorconfig could not be found"'; exit 1; }
        editorconfig "${1:-$kak_buffile}" | awk -F= -- '
            /indent_style=/            { indent_style = $2 }
            /indent_size=/             { indent_size = $2 == "tab" ? 4 : $2  }
            /tab_width=/               { tab_width = $2 }
            /end_of_line=/             { end_of_line = $2 }
            /charset=/                 { charset = $2 }
            /trim_trailing_whitespace=/ { trim_trailing_whitespace = $2 }

            END {
                if (indent_style == "tab") {
                    print "set-option buffer indentwidth 0"
                    print "set-option buffer aligntab true"
                }
                if (indent_style == "space") {
                    print "set-option buffer indentwidth " (indent_size == "tab" ? 4 : indent_size)
                    print "set-option buffer aligntab false"
                }
                if (indent_size || tab_width) {
                    print "set-option buffer tabstop " (tab_width ? tab_width : indent_size)
                }
                if (end_of_line == "lf" || end_of_line == "crlf") {
                    print "set-option buffer eolformat " end_of_line
                }
                if (charset == "utf-8-bom") {
                    print "set-option buffer BOM utf8"
                }
                if (trim_trailing_whitespace == "true") {
                    print "set-option buffer editorconfig_trim_trailing_whitespace true"
                }
            }
        '
    }
    hook buffer BufWritePre %val{buffile} -group editorconfig-hooks %{ evaluate-commands %sh{
        if [ ${kak_opt_editorconfig_trim_trailing_whitespace} = "true" ]; then
            printf %s\\n "try %{ execute-keys -draft %{ %s\h+$<ret>d } }"
        fi
    } }
}
