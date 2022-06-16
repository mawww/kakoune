# Syntax highlighting and indentation for Elvish (https://elv.sh)

hook global BufCreate .*\.elv %{
    set-option buffer filetype elvish
}

hook global WinSetOption filetype=elvish %<
    require-module elvish

    hook window ModeChange pop:insert:.* -group elvish-trim-indent elvish-trim-indent
    hook window InsertChar \n -group elvish-insert elvish-indent
    hook window InsertChar [\]})] -group elvish-insert elvish-deindent
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window elvish-.+ }
>

hook -group elvish-highlight global WinSetOption filetype=elvish %{
    add-highlighter window/elvish ref elvish
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/elvish }
}

provide-module elvish %§

add-highlighter shared/elvish regions
add-highlighter shared/elvish/code default-region group
add-highlighter shared/elvish/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/elvish/single_string region "'" ('')*' fill string
add-highlighter shared/elvish/comment region '#' $ fill comment

add-highlighter shared/elvish/code/variable regex \$[\w\d-_:~]+ 0:variable
add-highlighter shared/elvish/code/variable_in_assignment regex (?:^|\{\s|\(|\||\;)\h*(?:var|set|tmp|del)((?:\h+[\w\d-_:~]+)*) 1:variable
add-highlighter shared/elvish/code/variable_in_for regex (?:^|\{\s|\(|\||\;)\h*for\h+([\w\d-_:~]*) 1:variable
add-highlighter shared/elvish/code/variable_in_catch regex \}\h+(?:catch|except)\h+([\w\d-_:~]*) 1:variable

add-highlighter shared/elvish/code/builtin regex (?:^|\{\s|\(|\||\;)\h*(!=|!=s|%|\*|\+|-gc|-ifaddrs|-log|-override-wcwidth|-stack|-|/|<|<=|<=s|<s|==|==s|>|>=|>=s|>s|all|assoc|base|bool|break|call|cd|compare|constantly|continue|count|defer|deprecate|dissoc|drop|each|eawk|echo|eq|eval|exact-num|exec|exit|external|fail|fg|float64|from-json|from-lines|from-terminated|get-env|has-env|has-external|has-key|has-value|is|keys|kind-of|make-map|multi-error|nop|not-eq|not|ns|num|one|only-bytes|only-values|order|peach|pprint|print|printf|put|rand|randint|range|read-line|read-upto|repeat|repr|resolve|return|run-parallel|search-external|set-env|show|sleep|slurp|src|styled|styled-segment|take|tilde-abbr|time|to-json|to-lines|to-string|to-terminated|unset-env|use-mod|wcswidth)(?![-:])\b 1:builtin
add-highlighter shared/elvish/code/keyword regex (?:^|\{\s|\(|\||\;)\h*(use|var|set|tmp|del|and|or|coalesce|pragma|while|for|try|fn|if)(?![-:])\b 1:keyword
add-highlighter shared/elvish/code/keyword_block regex \}\h+(catch|elif|else|except|finally)(?![-:])\b 1:keyword

add-highlighter shared/elvish/code/metachar regex [*?|&\;<>()[\]{}] 0:operator

define-command -hidden elvish-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden elvish-indent %< evaluate-commands -draft -itersel %<
    execute-keys <semicolon>
    try %<
        # if the previous line is a comment, copy indent, # and whitespace
        execute-keys -draft k x s^\h*#\h*<ret> yjP
    > catch %<
        # copy indent
        execute-keys -draft K <a-&>
        # indent after { [ ( |
        try %< execute-keys -draft k x <a-k>[[{(|]\h*$<ret> j <a-gt> >
    >
>>

define-command -hidden elvish-deindent %< evaluate-commands -draft -itersel %<
    try %<
        # Deindent only when there is a lone closing character
        execute-keys -draft x <a-k>^\h*[^\h]$<ret> <a-lt>
    >
>>
§
