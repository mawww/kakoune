# Void Linux package template

provide-module detect-void-linux %{

hook global BufCreate .*/?srcpkgs/.+/template %{
    set-option buffer filetype sh
}

}

require-module detect-void-linux
