# https://gleam.run/
#
# a lot of this file was taken from rc/filetype/go.kak and rc/filetype/hare.kak, thanks everyone !

# Detection
hook global BufCreate .*\.gleam %{
    set-option buffer filetype gleam
}

# Initialization
hook global WinSetOption filetype=gleam %<
    require-module gleam
    set-option window static_words %opt{gleam_static_words}
    add-highlighter window/gleam ref gleam

    # remove whitespace when exiting insert mode
    hook window ModeChange pop:insert:.* -group gleam-trim-indent gleam-remove-white-space
    # gleam special indent hooks
    hook window InsertChar \n -group gleam-indent gleam-indent-new-line
    hook window InsertChar \n -group gleam-indent gleam-indent-after-blocks
    hook window InsertChar \} -group gleam-indent gleam-unindent-after-brackets
    hook window InsertChar \] -group gleam-indent gleam-unindent-after-brackets
    hook window InsertChar \) -group gleam-indent gleam-unindent-after-brackets
    # gleam special construct insert
    hook window InsertChar \n -group gleam-auto-insert gleam-insert-comment-on-new-line
    hook window InsertChar \n -group gleam-auto-insert gleam-insert-pipeline-on-new-line

    # Uninitialization
    hook -once -always window WinSetOption filetype=.* %[
        remove-highlighter window/gleam
        remove-hooks window gleam-.+
    ]
>

provide-module gleam %§
    # Highlighters
    add-highlighter shared/gleam regions
    add-highlighter shared/gleam/code default-region group
    add-highlighter shared/gleam/double_string region '"' (?<!\\)(\\\\)*" fill string
    add-highlighter shared/gleam/single_string region "'" (?<!\\)(\\\\)*' fill string
    add-highlighter shared/gleam/comment_line  region '//' $              fill comment

    add-highlighter shared/gleam/code/operator group
    add-highlighter shared/gleam/code/operator/basic      regex '(?:[-+/*]\.?|[=%])'  0:operator
    add-highlighter shared/gleam/code/operator/arrows     regex '(?:<-|[-|]>)'        0:operator
    add-highlighter shared/gleam/code/operator/comparison regex '(?:[<>]=?\.?|[=!]=)' 0:operator
    add-highlighter shared/gleam/code/operator/bool       regex '(?:&&|\|\|)'         0:operator
    add-highlighter shared/gleam/code/operator/misc       regex '(?:\.\.|<>|\|)'      0:operator

    add-highlighter shared/gleam/code/numeric group
    add-highlighter shared/gleam/code/numeric/natural     regex '0*[1-9](?:_?[0-9])*'                                                           0:value
    add-highlighter shared/gleam/code/numeric/real        regex '0*[1-9](?:_?[0-9])*(?:\.(?:0*[1-9](?:_?[0-9])*)?(?:e-?0*[1-9](?:_?[0-9])*)?)?' 0:value
    add-highlighter shared/gleam/code/numeric/binary      regex '\b0[bB]0*1(?:_?[01])*\b'                                                       0:value
    add-highlighter shared/gleam/code/numeric/octal       regex '\b0[oO]0*[1-7](?:_?[0-7])*\b'                                                  0:value
    add-highlighter shared/gleam/code/numeric/hexadecimal regex '\b0[xX]0*[1-9a-zA-Z](?:_?[0-9a-zA-Z])*\b'                                      0:value

    add-highlighter shared/gleam/code/function  regex '([a-z][0-9a-z_]*)\h*\('                                                              1:function
    add-highlighter shared/gleam/code/type      regex '[A-Z][a-zA-Z0-9]*'                                                                   0:type
    add-highlighter shared/gleam/code/keyword   regex '\b(as|assert|case|const|else|fn|if|import|let|opaque|panic|pub|todo|try|type|use)\b' 0:keyword
    add-highlighter shared/gleam/code/attribute regex '@[a-z][a-z_]*'                                                                       0:attribute

    declare-option str-list gleam_static_words \
        as assert case const else fn if import let opaque panic pub todo try type use

	define-command -hidden gleam-remove-white-space %[
		try %[ execute-keys -draft -itersel xs \h+$<ret>d ]
	]

    define-command -hidden gleam-indent-new-line %[
        try %[
            # align current line with previous indent
            execute-keys -draft -itersel <semicolon>K<a-&>
            # remove previous line extra whitespace
            evaluate-commands -draft -itersel %[
	            execute-keys k
	            gleam-remove-white-space
            ]
        ]
    ]
    define-command -hidden gleam-indent-after-blocks %<
        try %<
            # if there is already a closing bracket on the new created line, skip indentation
            execute-keys -draft -itersel xs ^\h*[\]})]$<ret>
        > catch %<
            try %<
	            execute-keys -draft -itersel kxs [[={(]$<ret>j<a-gt>
            >
        >
    >

    define-command -hidden gleam-unindent-after-brackets %[
        evaluate-commands -draft -itersel -no-hooks -save-regs x %[
            try %[
                try %[
                    execute-keys -draft h{c[{([],[})\]]<ret>gh<a-i><space>"xy
                ] catch %[ set-register x '' ]
                execute-keys -draft gh<a-i><space>"xR
            ]
        ]
    ]

    define-command -hidden gleam-insert-comment-on-new-line %[
        evaluate-commands -draft -itersel -no-hooks -save-regs x %[
            try %[
                execute-keys -draft kxs ^\h*/{2,}\h*<ret><a-f>/H"xy
                execute-keys -draft \"xP
            ]
        ]
    ]

    define-command -hidden gleam-insert-pipeline-on-new-line %[
        evaluate-commands -draft -itersel -no-hooks -save-regs x %[
            try %[
                execute-keys -draft kxs ^\h*\|>\h*<ret><a-f>|"xy
                execute-keys -draft \"xP
            ]
        ]
    ]
§

