# https://www.mercurial-scm.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*hg-editor-.*\.txt$ %{
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

add-highlighter shared/hg-commit regions
add-highlighter shared/hg-commit/comments region ^HG:\  $ group
add-highlighter shared/hg-commit/comments/ fill comment
add-highlighter shared/hg-commit/comments/ regex \
	"\b(?:(changed)|(removed)|(added)|(bookmark)|(branch)|(user:)) ([^\n]*)$" \
	      1:yellow  2:red     3:green 4:blue     5:magenta 6:white

}
