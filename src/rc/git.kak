hook global BufCreate .*COMMIT_EDITMSG \
    setb filetype git-commit

hook global WinSetOption filetype=git-commit \
    addhl group git-commit-highlight; \
    addhl -group git-commit-highlight regex "#[^\n]*\n" cyan default; \
    addhl -group git-commit-highlight regex "\<(modified|deleted|new file):[^\n]*\n" magenta default; \
    addhl -group git-commit-highlight regex "\<(modified|deleted|new file):" red default;

hook global WinSetOption filetype=(?!git-commit).* \
    rmhl git-commit-highlight

hook global BufCreate .*git-rebase-todo \
    setb filetype git-rebase

hook global WinSetOption filetype=git-rebase \
    addhl group git-rebase-highlight; \
    addhl -group git-rebase-highlight regex "#[^\n]*\n" cyan default; \
    addhl -group git-rebase-highlight regex "^(pick|edit|reword|squash|fixup|exec|[persfx]) \w+" magenta default; \
    addhl -group git-rebase-highlight regex "^(pick|edit|reword|squash|fixup|exec|[persfx])" green default;

hook global WinSetOption filetype=(?!git-rebase).* \
    rmhl git-rebase-highlight
