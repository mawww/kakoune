# http://gren-lang.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module detect-gren %{

hook global BufCreate .*[.](gren) %{
    set-option buffer filetype elm
}

}

require-module detect-gren

