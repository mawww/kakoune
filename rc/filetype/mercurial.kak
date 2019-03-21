# https://www.mercurial-scm.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*hg-editor-\w+\.txt$ %{
    set-option buffer filetype hg-commit
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

hook -group hg-commit-highlight global WinSetOption filetype=hg-commit %{
    add-highlighter window/ group hg-commit-highlight
    add-highlighter window/hg-commit-highlight regex '^HG:[^\n]*' 0:comment
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/hg-commit-highlight }
}
