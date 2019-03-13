# https://www.mercurial-scm.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*hg-editor-\w+\.txt$ %{
    set-option buffer filetype hg-commit
}

hook -once global BufSetOption filetype=hg-commit %{
    require-module hg-commit
}

provide-module hg-commit %{

# Faces
# ‾‾‾‾‾

set-face global MercurialCommitComment cyan

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

hook -group hg-commit-highlight global WinSetOption filetype=hg-commit %{
    add-highlighter window/ group hg-commit-highlight
    add-highlighter window/hg-commit-highlight regex '^HG:[^\n]*' 0:comment
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/hg-commit-highlight }
}

}
