# https://www.perl.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(t|p[lm])$ %{
    set-option buffer filetype perl
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=perl %{
    require-module perl

    set-option window static_words %opt{perl_static_words}

    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange pop:insert:.* -group perl-trim-indent %{ try %{ execute-keys -draft xs^\h+$<ret>d } }
    hook window InsertChar \n -group perl-insert perl-insert-on-new-line
    hook window InsertChar \n -group perl-indent perl-indent-on-new-line
    hook window InsertChar \{ -group perl-indent perl-indent-on-opening-curly-brace
    hook window InsertChar \} -group perl-indent perl-indent-on-closing-curly-brace

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window perl-.+ }
}

hook -group perl-highlight global WinSetOption filetype=perl %{
    add-highlighter window/perl ref perl
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/perl }
}

provide-module perl %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/perl regions
add-highlighter shared/perl/code default-region group
add-highlighter shared/perl/command        region (?<!\$)(?<!\\)`   (?<!\\)(\\\\)*` fill meta
add-highlighter shared/perl/double_string  region (?<!\$)"          (?<!\\)(\\\\)*" fill string
add-highlighter shared/perl/single_string  region (?<!\$)(?<!\\\\)' (?<!\\)(\\\\)*' fill string
add-highlighter shared/perl/comment        region (?<!\$)(?<!\\)#   $               fill comment

add-highlighter shared/perl/regex          region m?(?<!/)(?<!qr)/[^/\n]+(?=/)  /\w*   fill meta
add-highlighter shared/perl/sregex         region s/[^/\n]+/[^/\n]+(?=/)        /\w*   fill meta
add-highlighter shared/perl/bregex         region s\{[^\}\n]+\}\{[^\}\n]*(?=\}) \}\w*  fill meta

add-highlighter shared/perl/quote_brace    region -recurse \{ \bq[qrwx]?\{ \}          fill string
add-highlighter shared/perl/quote_paren    region -recurse \( \bq[qrwx]?\( \)          fill string
add-highlighter shared/perl/quote_brack    region -recurse \[ \bq[qrwx]?\[ \]          fill string
add-highlighter shared/perl/quote_angle    region -recurse  < \bq[qrwx]?<   >          fill string
add-highlighter shared/perl/quote_punct    region -match-capture '\bq[qwx]?([:;!@#$%^&*|,.?/~=+-])' '(.)' fill string
add-highlighter shared/perl/quote_regex    region -match-capture      '\bqr([:;!@#$%^&*|,.?/~=+-])' '(.)' fill meta

add-highlighter shared/perl/double_heredoc region -match-capture <<~?\h*'(\w*)' ^\t*(\w*)$ fill string
add-highlighter shared/perl/single_heredoc region -match-capture <<~?\h*"(\w*)" ^\t*(\w*)$ fill string
add-highlighter shared/perl/bare_heredoc   region -match-capture <<~?(\w+)      ^\t*(\w+)$ fill string
add-highlighter shared/perl/pod            region ^=\w+  ^=cut\b                            fill string

evaluate-commands %sh{
    # Grammar
    keywords="else lock qw elsif lt qx eq exp ne sub for no my not tr goto and foreach or break exit unless cmp ge package until continue gt while if qq xor do le qr return"
    attributes="END AUTOLOAD BEGIN CHECK UNITCHECK INIT DESTROY
                length setpgrp endgrent link setpriority endhostent listen setprotoent endnetent local setpwent
                endprotoent localtime setservent endpwent log setsockopt endservent lstat shift eof map shmctl eval mkdir shmget exec msgctl shmread
                exists msgget shmwrite msgrcv shutdown fcntl msgsnd sin fileno sleep flock next socket fork socketpair format oct sort
                formline open splice getc opendir split getgrent ord sprintf getgrgid our sqrt getgrnam pack srand gethostbyaddr pipe stat gethostbyname
                pop state gethostent pos study getlogin print substr getnetbyaddr printf symlink abs getnetbyname prototype syscall accept getnetent
                push sysopen alarm getpeername quotemeta sysread atan2 getpgrp rand sysseek getppid read system getpriority readdir syswrite bind
                getprotobyname readline tell binmode getprotobynumber readlink telldir bless getprotoent readpipe tie getpwent recv tied caller
                getpwnam redo time chdir getpwuid ref times getservbyname rename truncate chmod getservbyport require uc chomp getservent reset ucfirst
                chop getsockname umask chown getsockopt reverse undef chr glob rewinddir chroot gmtime rindex unlink close rmdir unpack
                closedir grep say unshift connect hex scalar untie cos index seek use crypt seekdir utime dbmclose int select values dbmopen ioctl semctl
                vec defined join semget wait delete keys semop waitpid kill send wantarray die last setgrent warn dump lc sethostent write each lcfirst setnetent"
    values="ARGV STDERR STDOUT ARGVOUT STDIN __DATA__ __END__ __FILE__ __LINE__ __PACKAGE__"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list perl_static_words $(join "${keywords} ${attributes} ${values}" ' ')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/perl/code/ regex \b($(join "${keywords}" '|'))\b 0:keyword
        add-highlighter shared/perl/code/ regex \b($(join "${attributes}" '|'))\b 0:attribute
        add-highlighter shared/perl/code/ regex \b($(join "${values}" '|'))\b 0:value
    "
}

add-highlighter shared/perl/code/ regex (?!\$)-?([0-9]*\.(?!0[xXbB]))?\b([0-9]+|0[xX][0-9a-fA-F]+|0[bb][01_]+)\.?([eE][+-]?[0-9]+)?i?\b 0:value
add-highlighter shared/perl/code/ regex %{\$!|\$"|\$#|\$\$|\$%|\$&|\$'|\$\(|\$\)|\$\*|\$\+|\$,|\$_|\$-|\$`|\$\.|\$/|\$:|\$;|\$<|\$=|\$>|\$\?|\$@|\$\[|\$\\|\$\]|\$\^|\$\||\$~|%!|@\+|@-|@_} 0:value
add-highlighter shared/perl/code/ regex (%ENV|%INC|%OVERLOAD|%SIG|@ARGV|@INC|@LAST_MATCH_START) 0:value
add-highlighter shared/perl/code/ regex %{%\^(H)\b} 0:value
add-highlighter shared/perl/code/ regex \$\^(S|T|V|W|X|A|C|D|E|F|H|I|L|M|N|O|P|R)\b 0:value
add-highlighter shared/perl/code/ regex \$\^(RE_TRIE_MAXBUF|TAINT|UNICODE|UTF8LOCALE|WARNING_BITS|WIDE_SYSTEM_CALLS|CHILD_ERROR_NATIVE|ENCODING|OPEN|RE_DEBUG_FLAGS)\b 0:value

add-highlighter shared/perl/code/ regex \$[0-9]+ 0:attribute
add-highlighter shared/perl/code/ regex \b-(B|b|C|c|d|e|f|g|k|l|M|O|o|p|r|R|S|s|T|t|u|w|W|X|x|z)\b 0:attribute

add-highlighter shared/perl/code/ regex \$[a-zA-Z_][a-zA-Z0-9_]* 0:variable

add-highlighter shared/perl/code/ regex \$(a|b|LAST_REGEXP_CODE_RESULT|LIST_SEPARATOR|MATCH|MULTILINE_MATCHING|NR|OFMT|OFS|ORS|OS_ERROR|OSNAME|OUTPUT_AUTO_FLUSH|OUTPUT_FIELD_SEPARATOR|OUTPUT_RECORD_SEPARATOR)\b 0:value
add-highlighter shared/perl/code/ regex \$(LAST_REGEXP_CODE_RESULT|LIST_SEPARATOR|MATCH|MULTILINE_MATCHING|NR|OFMT|OFS|ORS|OS_ERROR|OSNAME|OUTPUT_AUTO_FLUSH|OUTPUT_FIELD_SEPARATOR|OUTPUT_RECORD_SEPARATOR|PERL_VERSION|ACCUMULATOR|PERLDB|ARG|PID|ARGV|POSTMATCH|PREMATCH|BASETIME|PROCESS_ID|CHILD_ERROR|PROGRAM_NAME|COMPILING|REAL_GROUP_ID|DEBUGGING|REAL_USER_ID|EFFECTIVE_GROUP_ID|RS|EFFECTIVE_USER_ID|SUBSCRIPT_SEPARATOR|EGID|SUBSEP|ERRNO|SYSTEM_FD_MAX|EUID|UID|EVAL_ERROR|WARNING|EXCEPTIONS_BEING_CAUGHT|EXECUTABLE_NAME|EXTENDED_OS_ERROR|FORMAT_FORMFEED|FORMAT_LINE_BREAK_CHARACTERS|FORMAT_LINES_LEFT|FORMAT_LINES_PER_PAGE|FORMAT_NAME|FORMAT_PAGE_NUMBER|FORMAT_TOP_NAME|GID|INPLACE_EDIT|INPUT_LINE_NUMBER|INPUT_RECORD_SEPARATOR|LAST_MATCH_END|LAST_PAREN_MATCH)\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden perl-insert-on-new-line %~
    evaluate-commands -draft -itersel %=
        # copy # comments prefix and following white spaces
        try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K#\h* <ret> y<c-o>P<esc> }
    =
~

define-command -hidden perl-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft kx <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
        # indent after a switch's case/default statements
        try %[ execute-keys -draft kx <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after if|else|while|for
        try %[ execute-keys -draft <semicolon><a-F>)MB <a-k> \A(if|else|while|for)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-,><a-gt> ]
        # deindent closing brace(s) when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
    =
~

define-command -hidden perl-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{) <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden perl-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

§
