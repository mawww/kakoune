# package build description file

provide-module detect-arch-linux %{

hook global BufCreate (.*/)?PKGBUILD %{
    set-option buffer filetype sh
}

}

require-module detect-arch-linux
