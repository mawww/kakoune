# https://www.perl.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-perl %{
    set buffer filetype perl
}

hook global BufCreate .*\.pl %{
    set buffer filetype perl
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code perl \
    command '(?<!\$)`' (?<!\\)(\\\\)*` '' \
    double_string '(?<!\$)"' (?<!\\)(\\\\)*" '' \
    single_string "(?<!\$)'" (?<!\\)(\\\\)*' '' \
    comment '(?<!\$)#' $ ''

addhl -group /perl/command fill magenta
addhl -group /perl/double_string fill string
addhl -group /perl/single_string fill string
addhl -group /perl/comment fill comment

addhl -group /perl/code regex \<__(DATA|END|FILE|LINE|PACKAGE)__\> 0:value
addhl -group /perl/code regex \<(ARGV|STDERR|STDOUT|ARGVOUT|STDIN)\> 0:value
addhl -group /perl/code regex (?!\$)-?([0-9]*\.(?!0[xXbB]))?\<([0-9]+|0[xX][0-9a-fA-F]+|0[bb][01_]+)\.?([eE][+-]?[0-9]+)?i?\> 0:value
addhl -group /perl/code regex %{\$!|\$"|\$#|\$\$|\$%|\$&|\$'|\$\(|\$\)|\$\*|\$\+|\$,|\$_|\$-|\$`|\$\.|\$/|\$:|\$;|\$<|\$=|\$>|\$\?|\$@|\$\[|\$\\|\$\]|\$\^|\$\||\$~|%!|@\+|@-|@_} 0:value
addhl -group /perl/code regex (%ENV|%INC|%OVERLOAD|%SIG|@ARGV|@INC|@LAST_MATCH_START) 0:value
addhl -group /perl/code regex %{%\^(H)\>} 0:value
addhl -group /perl/code regex \$\^(S|T|V|W|X|A|C|D|E|F|H|I|L|M|N|O|P|R)\> 0:value
addhl -group /perl/code regex \$\^(RE_TRIE_MAXBUF|TAINT|UNICODE|UTF8LOCALE|WARNING_BITS|WIDE_SYSTEM_CALLS|CHILD_ERROR_NATIVE|ENCODING|OPEN|RE_DEBUG_FLAGS)\> 0:value

addhl -group /perl/code regex \$[0-9]+ 0:attribute
addhl -group /perl/code regex \<-(B|b|C|c|d|e|f|g|k|l|M|O|o|p|r|R|S|s|T|t|u|w|W|X|x|z)\> 0:attribute
addhl -group /perl/code regex \<(END|AUTOLOAD|BEGIN|CHECK|UNITCHECK|INIT|DESTROY)\> 0:attribute
addhl -group /perl/code regex \<(length|setpgrp|endgrent|link|setpriority|endhostent|listen|setprotoent|endnetent|local|setpwent)\> 0:attribute
addhl -group /perl/code regex \<(endprotoent|localtime|setservent|endpwent|log|setsockopt|endservent|lstat|shift|eof|map|shmctl|eval|mkdir|shmget|exec|msgctl|shmread)\> 0:attribute
addhl -group /perl/code regex \<(exists|msgget|shmwrite|msgrcv|shutdown|fcntl|msgsnd|sin|fileno|sleep|flock|next|socket|fork|socketpair|format|oct|sort)\> 0:attribute
addhl -group /perl/code regex \<(formline|open|splice|getc|opendir|split|getgrent|ord|sprintf|getgrgid|our|sqrt|getgrnam|pack|srand|gethostbyaddr|pipe|stat|gethostbyname)\> 0:attribute
addhl -group /perl/code regex \<(pop|state|gethostent|pos|study|getlogin|print|substr|getnetbyaddr|printf|symlink|abs|getnetbyname|prototype|syscall|accept|getnetent)\> 0:attribute
addhl -group /perl/code regex \<(push|sysopen|alarm|getpeername|quotemeta|sysread|atan2|getpgrp|rand|sysseek|getppid|read|system|getpriority|readdir|syswrite|bind)\> 0:attribute
addhl -group /perl/code regex \<(getprotobyname|readline|tell|binmode|getprotobynumber|readlink|telldir|bless|getprotoent|readpipe|tie|getpwent|recv|tied|caller)\> 0:attribute
addhl -group /perl/code regex \<(getpwnam|redo|time|chdir|getpwuid|ref|times|getservbyname|rename|truncate|chmod|getservbyport|require|uc|chomp|getservent|reset|ucfirst)\> 0:attribute
addhl -group /perl/code regex \<(chop|getsockname|umask|chown|getsockopt|reverse|undef|chr|glob|rewinddir|chroot|gmtime|rindex|unlink|close|rmdir|unpack)\> 0:attribute
addhl -group /perl/code regex \<(closedir|grep|say|unshift|connect|hex|scalar|untie|cos|index|seek|use|crypt|seekdir|utime|dbmclose|int|select|values|dbmopen|ioctl|semctl)\> 0:attribute
addhl -group /perl/code regex \<(vec|defined|join|semget|wait|delete|keys|semop|waitpid|kill|send|wantarray|die|last|setgrent|warn|dump|lc|sethostent|write|each|lcfirst|setnetent)\> 0:attribute

addhl -group /perl/code regex \<(else|lock|qw|elsif|lt|qx|eq||exp|ne|sub|for|no|my|not|tr|goto|and|foreach|or|break|exit|unless|cmp|ge|package|until|continue|gt|while|if|qq|xor|do|le|qr|return)\> 0:keyword

addhl -group /perl/code regex %{(?:\<[stqrmwy]+)?/[^\n/]*/([msixpodualngecr]+\>)?} 0:magenta
addhl -group /perl/code regex %{(?:\<[stqrmwy]+)?/[^\n/]+/[^\n/]*/([msixpeodualngcr]+\>)?} 0:magenta

addhl -group /perl/code regex \$[a-zA-Z_][a-zA-Z0-9_]* 0:blue

addhl -group /perl/code regex \$(a|b|LAST_REGEXP_CODE_RESULT|LIST_SEPARATOR|MATCH|MULTILINE_MATCHING|NR|OFMT|OFS|ORS|OS_ERROR|OSNAME|OUTPUT_AUTO_FLUSH|OUTPUT_FIELD_SEPARATOR|OUTPUT_RECORD_SEPARATOR)\> 0:value
addhl -group /perl/code regex \$(LAST_REGEXP_CODE_RESULT|LIST_SEPARATOR|MATCH|MULTILINE_MATCHING|NR|OFMT|OFS|ORS|OS_ERROR|OSNAME|OUTPUT_AUTO_FLUSH|OUTPUT_FIELD_SEPARATOR|OUTPUT_RECORD_SEPARATOR|PERL_VERSION|ACCUMULATOR|PERLDB|ARG|PID|ARGV|POSTMATCH|PREMATCH|BASETIME|PROCESS_ID|CHILD_ERROR|PROGRAM_NAME|COMPILING|REAL_GROUP_ID|DEBUGGING|REAL_USER_ID|EFFECTIVE_GROUP_ID|RS|EFFECTIVE_USER_ID|SUBSCRIPT_SEPARATOR|EGID|SUBSEP|ERRNO|SYSTEM_FD_MAX|EUID|UID|EVAL_ERROR|WARNING|EXCEPTIONS_BEING_CAUGHT|EXECUTABLE_NAME|EXTENDED_OS_ERROR|FORMAT_FORMFEED|FORMAT_LINE_BREAK_CHARACTERS|FORMAT_LINES_LEFT|FORMAT_LINES_PER_PAGE|FORMAT_NAME|FORMAT_PAGE_NUMBER|FORMAT_TOP_NAME|GID|INPLACE_EDIT|INPUT_LINE_NUMBER|INPUT_RECORD_SEPARATOR|LAST_MATCH_END|LAST_PAREN_MATCH)\> 0:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _perl-indent-on-new-line %~
    eval -draft -itersel %=
        # preserve previous line indent
        try %{ exec -draft \;K<a-&> }
        # indent after lines ending with { or (
        try %[ exec -draft k<a-x> <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ exec -draft k<a-x> s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ exec -draft [( <a-k> \`\([^\n]+\n[^\n]*\n?\' <ret> s \`\(\h*.|.\' <ret> '<a-;>' & }
        # copy // comments prefix
        try %{ exec -draft \;<c-s>k<a-x> s ^\h*\K/{2,} <ret> y<c-o><c-o>P<esc> }
        # indent after a switch's case/default statements
        try %[ exec -draft k<a-x> <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after if|else|while|for
        try %[ exec -draft \;<a-F>)MB <a-k> \`(if|else|while|for)\h*\(.*\)\h*\n\h*\n?\' <ret> s \`|.\' <ret> 1<a-&>1<a-space><a-gt> ]
    =
~

def -hidden _perl-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden _perl-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=perl %{
    addhl ref perl

    # cleanup trailing whitespaces when exiting insert mode
    hook window InsertEnd .* -group perl-hooks %{ try %{ exec -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group perl-indent _perl-indent-on-new-line
    hook window InsertChar \{ -group perl-indent _perl-indent-on-opening-curly-brace
    hook window InsertChar \} -group perl-indent _perl-indent-on-closing-curly-brace

    set window formatcmd "perltidy"
    set window comment_selection_chars ""
    set window comment_line_chars "#"
}

hook global WinSetOption filetype=(?!perl).* %{
    rmhl perl

    rmhooks window perl-hooks
    rmhooks window perl-indent
}
