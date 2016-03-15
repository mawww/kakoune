def editorconfig-load -docstring "Set indentation options according to editorconfig file" %{
    %sh{
        command -v editorconfig >/dev/null 2>&1 || { printf %s "echo -color Error The editorconfig tool could not be found"; exit 1; }
        editorconfig $kak_buffile | awk -F= -- \
            '{
                 if ($1 == "indent_style" && $2 == "tab") {
                     print "set buffer indentwidth 0" 
                 }
                 else if ($1 == "indent_size" && $2 ~ "[0-9]+") {
                     print "set buffer indentwidth", $2
                 }
                 else if ($1 == "tab_width" && $2 ~ "[0-9]+") {
                     print "set buffer tabstop", $2  
                 }
                 else if ($1 == "end_of_line") {
                     if ($2 == "lf" || $2 == "crlf") {
                        print "set buffer eolformat", $2
                     }
                     else {
                        print "echo -color yellow",$2,"is not a valid eolformat string: ignored\"" 
                     }
                 }
                 else if ($1 == "charset") {
                     if ($2 == "utf-8-bom") {
                         print "set buffer BOM utf8"
                     }
                     else {
                         print "set buffer BOM none"
                     }
                 }
             }'
    }
}
