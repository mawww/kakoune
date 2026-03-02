# portage ebuild file
provide-module detect-gentoo-linux %{
    
hook global BufCreate .*\.ebuild %{
    set-option buffer filetype sh
}

}

require-module detect-gentoo-linux
