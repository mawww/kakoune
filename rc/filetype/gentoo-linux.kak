# portage ebuild file
hook global BufCreate .*\.ebuild %{
    set-option buffer filetype sh
}
