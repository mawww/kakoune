# http://editorconfig.org/#file-format-details
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

decl -hidden bool editorconfig_trim_trailing_whitespace false

def editorconfig-load -params ..1 -docstring "editorconfig-load [file]: set formatting behavior according to editorconfig" %{
    remove-hooks buffer editorconfig-hooks
    %sh{
        command -v editorconfig >/dev/null 2>&1 || { echo 'echo -color Error editorconfig could not be found'; exit 1; }
        editorconfig "${1:-$kak_buffile}" | awk -F= -- '
            /indent_style=/            { indent_style = $2 }
            /indent_size=/             { indent_size = $2 == "tab" ? 4 : $2  }
            /tab_width=/               { tab_width = $2 }
            /end_of_line=/             { end_of_line = $2 }
            /charset=/                 { charset = $2 }
            /trim_trailing_whitespace/ { trim_trailing_whitespace = $2 }

            END {
                if (indent_style == "tab") {
                    print "set buffer indentwidth 0"
                    print "set buffer aligntab true"
                }
                if (indent_style == "space") {
                    print "set buffer indentwidth " (indent_size == "tab" ? 4 : indent_size)
                    print "set buffer aligntab false"
                }
                if (indent_size || tab_width) {
                    print "set buffer tabstop " (tab_width ? tab_width : indent_size)
                }
                if (end_of_line == "lf" || end_of_line == "crlf") {
                    print "set buffer eolformat " end_of_line
                }
                if (charset == "utf-8-bom") {
                    print "set buffer BOM utf8"
                }
                if (trim_trailing_whitespace == "true") {
                    print "set buffer editorconfig_trim_trailing_whitespace true"
                }
            }
        '
    }
    hook buffer BufWritePre %val{buffile} -group editorconfig-hooks %{ %sh{
        if [ ${kak_opt_editorconfig_trim_trailing_whitespace} = "true" ]; then
            printf %s\\n "try %{ exec -draft %{ %s\h+$<ret>d } }"
        fi
    } }
}
