# http://editorconfig.org/#file-format-details
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](editorconfig) %{
    set-option buffer filetype ini
    set-option buffer static_words indent_style indent_size tab_width \
    end_of_line charset insert_final_newline trim_trailing_whitespace root \
    latin1 utf-8 utf-8-bom utf-16be utf-16le lf cr crlf unset space tab max_line_length
}

define-command editorconfig-load -params ..1 -docstring "editorconfig-load [file]: set formatting behavior according to editorconfig" %{
    evaluate-commands %sh{
        command -v editorconfig >/dev/null 2>&1 || { echo "fail editorconfig could not be found"; exit 1; }

        file="${1:-$kak_buffile}"
        case $file in
            /*) # $kak_buffile is a full path that starts with a '/'
                printf %s\\n "remove-hooks buffer editorconfig-hooks"
                editorconfig "$file" | awk -v file="$file" -F= -- '
                    $1 == "indent_style"             { indent_style = $2 }
                    $1 == "indent_size"              { indent_size = $2 == "tab" ? 4 : $2 }
                    $1 == "tab_width"                { tab_width = $2 }
                    $1 == "end_of_line"              { end_of_line = $2 }
                    $1 == "charset"                  { charset = $2 }
                    $1 == "trim_trailing_whitespace" { trim_trailing_whitespace = $2 }
                    $1 == "max_line_length"          { max_line_length = $2 }

                    END {
                        if (indent_style == "tab") {
                            print "set-option buffer indentwidth 0"
                        }
                        if (indent_style == "space") {
                            print "set-option buffer indentwidth " indent_size
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
                            print "hook buffer BufWritePre \"" file "\" -group editorconfig-hooks %{ try %{ execute-keys -draft %{%s\\h+$|\\n+\\z<ret>d} } }"
                        }
                        if (max_line_length && max_line_length != "off") {
                            print "set window autowrap_column " max_line_length
                            print "autowrap-enable"
                            print "add-highlighter window/ column %sh{ echo $((" max_line_length "+1)) } default,bright-black"
                        }
                    }
                ' ;;
        esac
    }
}
complete-command editorconfig-load file
