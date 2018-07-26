# https://www.mercurial-scm.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Faces
# ‾‾‾‾‾

set-face global MercurialCommitComment cyan

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*hg-editor-\w+\.txt$ %{
    set-option buffer filetype hg-commit
}

hook -group hg-commit-highlight global WinSetOption filetype=(?!hg-commit).* %{
    remove-highlighter window/hg-commit-highlight
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

hook -group hg-commit-highlight global WinSetOption filetype=hg-commit %{
    add-highlighter window/ group hg-commit-highlight
    add-highlighter window/hg-commit-highlight regex '^HG:[^\n]*' 0:MercurialCommitComment
}
