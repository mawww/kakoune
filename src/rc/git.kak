hook global WinCreate .*COMMIT_EDITMSG \
    addhl group hlgit; \
    addgrouphl hlgit regex "#[^\n]*\n" cyan default; \
    addgrouphl hlgit regex "\<(modified|deleted|new file):[^\n]*\n" magenta default; \
    addgrouphl hlgit regex "\<(modified|deleted|new file):" red default;

hook global WinCreate .*git-rebase-todo \
    addhl group hlgit; \
    addgrouphl hlgit regex "#[^\n]*\n" cyan default; \
    addgrouphl hlgit regex "^(pick|edit|reword|squash|fixup|exec|[persfx]) \w+" magenta default; \
    addgrouphl hlgit regex "^(pick|edit|reword|squash|fixup|exec|[persfx])" green default;

