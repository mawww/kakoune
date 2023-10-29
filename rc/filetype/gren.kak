# http://gren-lang.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](gren) %{
    set-option buffer filetype elm
}

