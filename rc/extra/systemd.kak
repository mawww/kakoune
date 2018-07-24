# https://freedesktop.org/wiki/Software/systemd/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .+\.(automount|conf|link|mount|network|path|service|slice|socket|target|timer) %{
    set-option buffer filetype ini
}
