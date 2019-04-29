# https://www.mercurial-scm.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*hg-editor-\w+\.txt$ %{
    set-option buffer filetype hg-commit
}

hook global WinSetOption filetype=hg-commit %{
    require-module hg-commit
}

hook -group hg-commit-highlight global WinSetOption filetype=hg-commit %{
    add-highlighter window/hg-commit ref hg-commit
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/hg-commit-highlight }
}

provide-module hg-commit %{

# Faces
# ‾‾‾‾‾

set-face global MercurialCommitComment cyan

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/hg-commit group
add-highlighter shared/hg-commit/ regex '^HG:[^\n]*' 0:comment

}
