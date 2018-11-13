# Void Linux package template
hook global BufCreate .*/?srcpkgs/.+/template %{
    set-option buffer filetype sh
}
