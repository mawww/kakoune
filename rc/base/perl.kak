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

%sh{
    # Grammar
    keywords="else|lock|qw|elsif|lt|qx|eq|exp|ne|sub|for|no|my|not|tr|goto|and|foreach|or|break|exit|unless|cmp|ge|package|until|continue|gt|while|if|qq|xor|do|le|qr|return"
    attributes="END|AUTOLOAD|BEGIN|CHECK|UNITCHECK|INIT|DESTROY"
    attributes="${attributes}|length|setpgrp|endgrent|link|setpriority|endhostent|listen|setprotoent|endnetent|local|setpwent"
    attributes="${attributes}|endprotoent|localtime|setservent|endpwent|log|setsockopt|endservent|lstat|shift|eof|map|shmctl|eval|mkdir|shmget|exec|msgctl|shmread"
    attributes="${attributes}|exists|msgget|shmwrite|msgrcv|shutdown|fcntl|msgsnd|sin|fileno|sleep|flock|next|socket|fork|socketpair|format|oct|sort"
    attributes="${attributes}|formline|open|splice|getc|opendir|split|getgrent|ord|sprintf|getgrgid|our|sqrt|getgrnam|pack|srand|gethostbyaddr|pipe|stat|gethostbyname"
    attributes="${attributes}|pop|state|gethostent|pos|study|getlogin|print|substr|getnetbyaddr|printf|symlink|abs|getnetbyname|prototype|syscall|accept|getnetent"
    attributes="${attributes}|push|sysopen|alarm|getpeername|quotemeta|sysread|atan2|getpgrp|rand|sysseek|getppid|read|system|getpriority|readdir|syswrite|bind"
    attributes="${attributes}|getprotobyname|readline|tell|binmode|getprotobynumber|readlink|telldir|bless|getprotoent|readpipe|tie|getpwent|recv|tied|caller"
    attributes="${attributes}|getpwnam|redo|time|chdir|getpwuid|ref|times|getservbyname|rename|truncate|chmod|getservbyport|require|uc|chomp|getservent|reset|ucfirst"
    attributes="${attributes}|chop|getsockname|umask|chown|getsockopt|reverse|undef|chr|glob|rewinddir|chroot|gmtime|rindex|unlink|close|rmdir|unpack"
    attributes="${attributes}|closedir|grep|say|unshift|connect|hex|scalar|untie|cos|index|seek|use|crypt|seekdir|utime|dbmclose|int|select|values|dbmopen|ioctl|semctl"
    attributes="${attributes}|vec|defined|join|semget|wait|delete|keys|semop|waitpid|kill|send|wantarray|die|last|setgrent|warn|dump|lc|sethostent|write|each|lcfirst|setnetent"
    values="ARGV|STDERR|STDOUT|ARGVOUT|STDIN|__DATA__|__END__|__FILE__|__LINE__|__PACKAGE__"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=perl %{
        set window static_words '${keywords}:${attributes}:${values}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /perl/code regex \b(${keywords})\b 0:keyword
        addhl -group /perl/code regex \b(${attributes})\b 0:attribute
        addhl -group /perl/code regex \b(${values})\b 0:value
    "
}

addhl -group /perl/code regex (?!\$)-?([0-9]*\.(?!0[xXbB]))?\b([0-9]+|0[xX][0-9a-fA-F]+|0[bb][01_]+)\.?([eE][+-]?[0-9]+)?i?\b 0:value
addhl -group /perl/code regex %{\$!|\$"|\$#|\$\$|\$%|\$&|\$'|\$\(|\$\)|\$\*|\$\+|\$,|\$_|\$-|\$`|\$\.|\$/|\$:|\$;|\$<|\$=|\$>|\$\?|\$@|\$\[|\$\\|\$\]|\$\^|\$\||\$~|%!|@\+|@-|@_} 0:value
addhl -group /perl/code regex (%ENV|%INC|%OVERLOAD|%SIG|@ARGV|@INC|@LAST_MATCH_START) 0:value
addhl -group /perl/code regex %{%\^(H)\b} 0:value
addhl -group /perl/code regex \$\^(S|T|V|W|X|A|C|D|E|F|H|I|L|M|N|O|P|R)\b 0:value
addhl -group /perl/code regex \$\^(RE_TRIE_MAXBUF|TAINT|UNICODE|UTF8LOCALE|WARNING_BITS|WIDE_SYSTEM_CALLS|CHILD_ERROR_NATIVE|ENCODING|OPEN|RE_DEBUG_FLAGS)\b 0:value

addhl -group /perl/code regex \$[0-9]+ 0:attribute
addhl -group /perl/code regex \b-(B|b|C|c|d|e|f|g|k|l|M|O|o|p|r|R|S|s|T|t|u|w|W|X|x|z)\b 0:attribute

addhl -group /perl/code regex %{(?:\b[stqrmwy]+)?/[^\n/]*/([msixpodualngecr]+\b)?} 0:magenta
addhl -group /perl/code regex %{(?:\b[stqrmwy]+)?/[^\n/]+/[^\n/]*/([msixpeodualngcr]+\b)?} 0:magenta

addhl -group /perl/code regex \$[a-zA-Z_][a-zA-Z0-9_]* 0:blue

addhl -group /perl/code regex \$(a|b|LAST_REGEXP_CODE_RESULT|LIST_SEPARATOR|MATCH|MULTILINE_MATCHING|NR|OFMT|OFS|ORS|OS_ERROR|OSNAME|OUTPUT_AUTO_FLUSH|OUTPUT_FIELD_SEPARATOR|OUTPUT_RECORD_SEPARATOR)\b 0:value
addhl -group /perl/code regex \$(LAST_REGEXP_CODE_RESULT|LIST_SEPARATOR|MATCH|MULTILINE_MATCHING|NR|OFMT|OFS|ORS|OS_ERROR|OSNAME|OUTPUT_AUTO_FLUSH|OUTPUT_FIELD_SEPARATOR|OUTPUT_RECORD_SEPARATOR|PERL_VERSION|ACCUMULATOR|PERLDB|ARG|PID|ARGV|POSTMATCH|PREMATCH|BASETIME|PROCESS_ID|CHILD_ERROR|PROGRAM_NAME|COMPILING|REAL_GROUP_ID|DEBUGGING|REAL_USER_ID|EFFECTIVE_GROUP_ID|RS|EFFECTIVE_USER_ID|SUBSCRIPT_SEPARATOR|EGID|SUBSEP|ERRNO|SYSTEM_FD_MAX|EUID|UID|EVAL_ERROR|WARNING|EXCEPTIONS_BEING_CAUGHT|EXECUTABLE_NAME|EXTENDED_OS_ERROR|FORMAT_FORMFEED|FORMAT_LINE_BREAK_CHARACTERS|FORMAT_LINES_LEFT|FORMAT_LINES_PER_PAGE|FORMAT_NAME|FORMAT_PAGE_NUMBER|FORMAT_TOP_NAME|GID|INPLACE_EDIT|INPUT_LINE_NUMBER|INPUT_RECORD_SEPARATOR|LAST_MATCH_END|LAST_PAREN_MATCH)\b 0:value

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

hook -group perl-highlight global WinSetOption filetype=perl %{ addhl ref perl }

hook global WinSetOption filetype=perl %{
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
