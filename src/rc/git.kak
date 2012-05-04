hook global WinCreate .*COMMIT_EDITMSG \
    addhl group hlgit; \
    addhl -group hlgit regex "#[^\n]*\n" cyan default; \
    addhl -group hlgit regex "\<(modified|deleted|new file):[^\n]*\n" magenta default; \
    addhl -group hlgit regex "\<(modified|deleted|new file):" red default;

hook global WinCreate .*git-rebase-todo \
    addhl group hlgit; \
    addhl -group hlgit regex "#[^\n]*\n" cyan default; \
    addhl -group hlgit regex "^(pick|edit|reword|squash|fixup|exec|[persfx]) \w+" magenta default; \
    addhl -group hlgit regex "^(pick|edit|reword|squash|fixup|exec|[persfx])" green default;

