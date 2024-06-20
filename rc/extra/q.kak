
# Detection
hook global BufCreate .*\.(q|k) %{
    set-option buffer filetype q
}

hook global BufCreate .*\.q.in %{
    set-option buffer filetype q
}

rmhl shared/q
add-highlighter shared/q regions

add-highlighter shared/q/code default-region group
add-highlighter shared/q/string region '"'   (?<!\\)(\\\\)*"  fill string
add-highlighter shared/q/comment region '^/\h*$'   ^\Q\\E\h*$  fill comment

add-highlighter shared/q/comment1 region  ' /'   '$' fill comment
add-highlighter shared/q/comment2 region  '^\Q\\E\h*$'   'this match to eof' fill comment
add-highlighter shared/q/comment3 region  '^/'   '$' fill comment

# Float formats
add-highlighter shared/q/code/int regex '\b\d+[eE][+-]?\d+[ef]?\b' 0:value
add-highlighter shared/q/code/ regex '(\b\d+)?\.\d+[ef]?\b' 0:value
add-highlighter shared/q/code/ regex '\b\d+\.[ef]?' 0:value
#inf and null
add-highlighter shared/q/code/ regex '\b0[NW][hijepdnuvt]?\b' 0:value
add-highlighter shared/q/code/ regex '\b0[nw]\b' 0:value
add-highlighter shared/q/code/ regex '\b0N[gm]\b' 0:value


# integers
add-highlighter shared/q/code/ regex '\b([1-9]\d*|0)[hij]?\b' 0:value
add-highlighter shared/q/code/ regex '\b[01]+b\b' 0:value
add-highlighter shared/q/code/ regex '(?i)\b0x[\da-f]+\b' 0:value


hook -group q-highlight global WinSetOption filetype=q %{ add-highlighter window/q ref q }
hook -group q-highlight global WinSetOption filetype=(?!q).* %{ remove-highlighter window/q }

evaluate-commands %sh<
    keywords="abs|acos|asin|atan|avg|bin|by|binr|cor|cos|cov"
    keywords="${keywords}|delete|dev|div|do|enlist|exec|exit|exp|getenv|if"
    keywords="${keywords}|in|insert|last|like|log|max|min|prd|select|setenv"
    keywords="${keywords}|sin|sqrt|ss|sum|tan|update|var|wavg|while|within"
    keywords="${keywords}|wsum|xexp||neg|not|null|string|reciprocal|floor|ceiling"
    keywords="${keywords}|signum|mod|xbar|xlog|and|or|each|scan|over|prior"
    keywords="${keywords}|mmu|lsq|inv|md5|ltime|gtime|count|first|svar|sdev"
    keywords="${keywords}|scov|med|all|any|rand|sums|prds|mins|maxs|fills"
    keywords="${keywords}|deltas|ratios|avgs|differ|prev|next|rank|reverse|iasc|idesc"
    keywords="${keywords}|asc|desc|msum|mcount|mavg|mdev|xrank|mmin|mmax|xprev"
    keywords="${keywords}|rotate|ema|distinct|group|where|flip|type|key|til|get"
    keywords="${keywords}|value|attr|cut|set|upsert|raze|union|inter|except|cross"
    keywords="${keywords}|sv|vs|sublist|read0|read1|hopen|hclose|hdel|hsym|hcount"
    keywords="${keywords}|peach|system|ltrim|rtrim|trim|lower|upper|ssr|view|tables"
    keywords="${keywords}|views|cols|xcols|keys|xkey|xcol|xasc|xdesc|fkeys|meta"
    keywords="${keywords}|lj|ljf|aj|aj0|ij|ijf|pj|asof|uj|ujf"
    keywords="${keywords}|ww|wj|wj1|fby|xgroup|ungroup|ej|save|load|rsave"
    keywords="${keywords}|rload|dsave|show|csv|parse|eval|reval|from|;"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=q %{
        set-option window static_words ${keywords}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/q/code/ regex '\b(${keywords})\b' 0:keyword
        add-highlighter shared/q/code/ regex '([a-zA-Z]\w*)\s*:\s*\{' 1:function
        add-highlighter shared/q/code/ regex '(;)' 0:keyword
        add-highlighter shared/q/code/ regex '(\`[.:_a-zA-Z0-9/]*))' 0:type
    "
>

add-highlighter shared/q/code/ regex (?<=[\w\s\d'"_])(=|<>|~|<|<=|>|>=|\+|-|\*|%|#|,|\^|_|\||&) 0:operator

define-command -hidden -override q-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k <a-x> s \h+$ <ret> d }
    }
}

hook global WinSetOption filetype=q %{
    hook window InsertChar \n -group q-indent q-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange insert:.* -group q-indent %{ try %{ execute-keys -draft \; <a-x> s ^\h+$ <ret> d } }
    face window function default+b
}

hook global WinSetOption filetype=(?!q).* %{
    remove-hooks window q-indent
}
