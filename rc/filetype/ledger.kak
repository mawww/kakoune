# Detection
# ---------

# The .ledger suffix is not required by ledger, but the best I can do.
hook global BufCreate .*\.ledger %{
    set-option buffer filetype ledger
}

# Initialization
# --------------

hook global WinSetOption filetype=ledger %{
    require-module ledger

    hook window InsertChar \n -group ledger-indent ledger-indent-on-new-line
    hook window ModeChange pop:insert:.* -group ledger-trim-indent ledger-trim-indent

    hook -once -always window WinSetOption filetype=.* %{
        remove-hooks window ledger-.+
        unset-option window static_words  # Remove static completion
    }
}

hook -group ledger-highlight global WinSetOption filetype=ledger %{
    add-highlighter window/ledger ref ledger
    hook -once -always window WinSetOption filetype=.* %{
        remove-highlighter window/ledger
    }
}

# Completion
# ----------

hook -group ledger-complete global WinSetOption filetype=ledger %{
    set-option window static_words account note alias payee check assert eval \
        default apply fixed bucket capture comment commodity format nomarket \
        define end include tag test year
}

provide-module ledger %[

# Highlighters
# ------------
#
# TODO: highlight tag comments

add-highlighter shared/ledger regions

# The following highlighters implement
# https://www.ledger-cli.org/3.0/doc/ledger3.html#Transactions-and-Comments

add-highlighter shared/ledger/transaction region '^[0-9]' '^(?=\H)' group
add-highlighter shared/ledger/transaction/first_line regex \
    '^([0-9].*?)\h.*?((  +|\t+);.*?)?$' 1:function 2:string
add-highlighter shared/ledger/transaction/posting regex \
    '^\h+([^\h;].*?)((  +|\t+).*?)?((  +|\t+);.*?)?$' 1:type 2:value 4:string
add-highlighter shared/ledger/transaction/note regex '^\h+;[^$]*?$' 0:string

add-highlighter shared/ledger/comment region '^(;|#|%|\||\*)' '$' fill comment

# TODO: Improve
add-highlighter shared/ledger/other region '^(P|=|~)' '$' fill meta

# The following highlighters implement
# https://www.ledger-cli.org/3.0/doc/ledger3.html#Command-Directives

add-highlighter shared/ledger/default default-region group

# Add highlighters for simple one-line command directives
evaluate-commands %sh{
    # TODO: Is `expr` also a command directive? The documentation confuses me.
    for cmd in 'apply account' 'apply fixed' 'assert' 'bucket' 'check' 'end' \
               'include' 'apply tag' 'test' 'year'; do
        echo "add-highlighter shared/ledger/default/ regex '^${cmd}\b' 0:function"
    done
}

add-highlighter shared/ledger/account region '^account' '^(?=\H)' group
add-highlighter shared/ledger/account/first_line regex '^account'    0:function
add-highlighter shared/ledger/account/note       regex '^\h*note'    0:function
add-highlighter shared/ledger/account/alias      regex '^\h*alias'   0:function
add-highlighter shared/ledger/account/payee      regex '^\h*payee'   0:function
add-highlighter shared/ledger/account/check      regex '^\h*check'   0:function
add-highlighter shared/ledger/account/assert     regex '^\h*assert'  0:function
add-highlighter shared/ledger/account/eval       regex '^\h*eval'    0:function
add-highlighter shared/ledger/account/default    regex '^\h*default' 0:function

add-highlighter shared/ledger/alias region '^alias' '$' group
add-highlighter shared/ledger/alias/keyword regex '^alias' 0:function
add-highlighter shared/ledger/alias/key regex '^alias\h([^$=]*)=?' 1:variable
add-highlighter shared/ledger/alias/value regex '^alias\h.*?=(.*?)$' 1:value

add-highlighter shared/ledger/capture region '^capture' '$' group
add-highlighter shared/ledger/capture/keyword regex '^capture' 0:function
add-highlighter shared/ledger/capture/account regex \
    '^capture\h+(.*?)(  +|\t+|$)' 1:type
add-highlighter shared/ledger/capture/regex regex \
    '^capture\h+.*?(  +|\t+)(.*?)$' 2:value

add-highlighter shared/ledger/comment_block region '^comment' '^end comment' \
    fill comment

add-highlighter shared/ledger/commodity region '^commodity' '^(?=\H)' group
add-highlighter shared/ledger/commodity/first_line regex '^commodity'   0:function
add-highlighter shared/ledger/commodity/note       regex '^\h*note'     0:function
add-highlighter shared/ledger/commodity/format     regex '^\h*format'   0:function
add-highlighter shared/ledger/commodity/nomarket   regex '^\h*nomarket' 0:function
add-highlighter shared/ledger/commodity/alias      regex '^\h*alias'    0:function
add-highlighter shared/ledger/commodity/default    regex '^\h*default'  0:function

add-highlighter shared/ledger/define region '^define' '$' group
add-highlighter shared/ledger/define/keyword regex '^define' 0:function
add-highlighter shared/ledger/define/key regex '^define\h([^$=]*)=?' 1:variable
add-highlighter shared/ledger/define/value regex '^define\h.*?=(.*?)$' 1:value

add-highlighter shared/ledger/payee region '^payee' '^(?=\H)' group
add-highlighter shared/ledger/payee/first_line regex '^payee'    0:function
add-highlighter shared/ledger/payee/alias      regex '^\h*alias' 0:function
add-highlighter shared/ledger/payee/uuid       regex '^\h*uuid'  0:function

add-highlighter shared/ledger/tag region '^tag' '^(?=\H)' group
add-highlighter shared/ledger/tag/first_line regex '^tag'       0:function
add-highlighter shared/ledger/tag/check      regex '^\h*check'  0:function
add-highlighter shared/ledger/tag/assert     regex '^\h*assert' 0:function

# Commands
# --------

define-command -hidden ledger-indent-on-new-line %[
    evaluate-commands -draft -itersel %[
        # preserve previous line indent
        try %[ execute-keys -draft <semicolon> K <a-&> ]
        # cleanup trailing whitespaces from previous line
        try %[ execute-keys -draft k x s \h+$ <ret> d ]
        # indent after the first line of a transaction
        try %[ execute-keys -draft kx <a-k>^[0-9]<ret> j<a-gt> ]
    ]
]

define-command -hidden ledger-trim-indent %{
    try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d }
}

]
