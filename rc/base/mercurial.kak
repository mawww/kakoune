# https://www.mercurial-scm.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Faces
# ‾‾‾‾‾

face MercurialCommitComment cyan

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*hg-editor-\w+\.txt$ %{
    set buffer filetype hg-commit
}

hook -group hg-commit-highlight global WinSetOption filetype=(?!hg-commit).* %{
    remove-highlighter hg-commit-highlight
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

hook -group hg-commit-highlight global WinSetOption filetype=hg-commit %{
    add-highlighter group hg-commit-highlight
    add-highlighter -group hg-commit-highlight regex '^HG:[^\n]*' 0:MercurialCommitComment
}
