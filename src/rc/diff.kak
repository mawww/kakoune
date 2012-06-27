hook global BufCreate .*\.diff \
    setb filetype diff

hook global WinSetOption filetype=diff \
    addhl group diff-highlight; \
    addhl -group diff-highlight regex "^\+[^\n]*\n" green default; \
    addhl -group diff-highlight regex "^-[^\n]*\n" red default; \
    addhl -group diff-highlight regex "^@@[^\n]*@@" cyan default;

hook global WinSetOption filetype=(?!diff).* \
    rmhl diff-highlight
