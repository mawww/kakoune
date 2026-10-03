# package build description file
hook global BufCreate (.*/)?PKGBUILD %{
    set-option buffer filetype sh
}

# makepkg configuration files
hook global BufCreate .*.?makepkg.conf %{
    set-option buffer filetype sh
}
hook global BufCreate .*/etc/makepkg.conf.d/.*.conf %{
    set-option buffer filetype sh
}
