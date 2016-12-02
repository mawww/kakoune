def editorconfig-load -docstring "Set indentation options according to editorconfig file" %{
    %sh{
        command -v editorconfig >/dev/null 2>&1 || { echo 'echo -color Error The editorconfig tool could not be found'; exit 1; }
        editorconfig $kak_buffile | awk -F= -- '
            /indent_style=/ { indent_style = $2 }
            /indent_size=/  { indent_size = $2 == "tab" ? 4 : $2  }
            /tab_width=/    { tab_width = $2 }
            /end_of_line=/  { end_of_line = $2 }
            /charset=/      { charset = $2 }

            END {
                if (indent_style == "tab")
                {
                    print "set buffer indentwidth 0"
                    print "set buffer aligntab true"
                }
                if (indent_style == "spaces") {
                    print "set buffer indentwidth " (indent_size == "tab" ? 4 : indent_size)
                    print "set buffer aligntab false"
                }
                if (indent_size || tab_width) {
                    print "set buffer tabstop " (tab_width ? tab_width : indent_size)
                }
                if (end_of_line) {
                    if (end_of_line == "lf" || end_of_line == "crlf")
                        print "set buffer eolformat " end_of_line
                    else
                        print "error"
                }
                if (charset)
                    print "set buffer BOM" (charset == "utf-8-bom" ? true : false)
            }
        '
    }
}
