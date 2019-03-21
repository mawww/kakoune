# https://freedesktop.org/wiki/Software/systemd/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*/systemd/.+\.(automount|conf|link|mount|network|path|service|slice|socket|target|timer) %{
    set-option buffer filetype ini

    # NOTE: INI files define the commenting character to be `;`, which won't work in `systemd` files
    hook -once buffer BufSetOption comment_line=.+ %{
        set-option buffer comment_line "#"
    }
}
