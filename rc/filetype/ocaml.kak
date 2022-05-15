# http://ocaml.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.mli? %{
    set-option buffer filetype ocaml
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ocaml %{
    require-module ocaml
    set-option window static_words %opt{ocaml_static_words}
}

hook -group ocaml-highlight global WinSetOption filetype=ocaml %{
    add-highlighter window/ocaml ref ocaml
    alias window alt ocaml-alternative-file
    hook -once -always window WinSetOption filetype=.* %{
        unalias window alt ocaml-alternative-file
        remove-highlighter window/ocaml
    }
}

provide-module ocaml %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ocaml regions
add-highlighter shared/ocaml/code default-region group
add-highlighter shared/ocaml/string region (?<!['\\])" (?<!\\)(\\\\)*" fill string
add-highlighter shared/ocaml/quotedstring region -match-capture %"\{(\w*)\|" %"\|(\w*)\}" fill string
add-highlighter shared/ocaml/comment region -recurse \Q(* \Q(* \Q*) fill comment

add-highlighter shared/ocaml/code/char regex %{\B'([^'\\]|(\\[\\"'nrtb])|(\\\d{3})|(\\x[a-fA-F0-9]{2})|(\\o[0-7]{3}))'\B} 0:value

# integer literals
add-highlighter shared/ocaml/code/ regex \b[0-9][0-9_]*([lLn]?)\b                          0:value
add-highlighter shared/ocaml/code/ regex \b0[xX][A-Fa-f0-9][A-Fa-f0-9_]*([lLn]?)\b         0:value
add-highlighter shared/ocaml/code/ regex \b0[oO][0-7][0-7_]*([lLn]?)\b                     0:value
add-highlighter shared/ocaml/code/ regex \b0[bB][01][01_]*([lLn]?)\b                       0:value
# float literals
add-highlighter shared/ocaml/code/ regex \b[0-9][0-9_]*(\.[0-9_]*)?([eE][+-]?[0-9][0-9_]*)?                       0:value
add-highlighter shared/ocaml/code/ regex \b0[xX][A-Fa-f0-9][A-Fa-f0-9]*(\.[a-fA-F0-9_]*)?([pP][+-]?[0-9][0-9_]*)? 0:value
# constructors. must be put before any module name highlighter, as a fallback for capitalized identifiers
add-highlighter shared/ocaml/code/ regex \b[A-Z][a-zA-Z0-9_]*\b                            0:value
# the module name in a module path, e.g. 'M' in 'M.x'
add-highlighter shared/ocaml/code/ regex (\b[A-Z][a-zA-Z0-9_]*[\h\n]*)(?=\.)               0:module
# (simple) module declarations
add-highlighter shared/ocaml/code/ regex (?<=module)([\h\n]+[A-Z][a-zA-Z0-9_]*)            0:module
# (simple) signature declarations. 'type' is also highlighted, due to the lack of quantifiers in lookarounds.
# Hence we must put keyword highlighters after this to recover proper highlight for 'type'
add-highlighter shared/ocaml/code/ regex (?<=module)([\h\n]+type[\h\n]+[A-Z][a-zA-Z0-9_]*) 0:module
# (simple) open statements
add-highlighter shared/ocaml/code/ regex (?<=open)([\h\n]+[A-Z][a-zA-Z0-9_]*)              0:module
# operators
add-highlighter shared/ocaml/code/ regex [@!$%%^&*\-+=|<>/?]+                              0:operator


# Macro
# ‾‾‾‾‾

evaluate-commands %sh{
  keywords="and|as|asr|assert|begin|class|constraint|do|done|downto|else|end|exception|external|false"
  keywords="${keywords}|for|fun|function|functor|if|in|include|inherit|initializer|land|lazy|let|lor"
  keywords="${keywords}|lsl|lsr|lxor|match|method|mod|module|mutable|new|nonrec|object|of|open|or"
  keywords="${keywords}|private|rec|sig|struct|then|to|true|try|type|val|virtual|when|while|with"

  printf %s\\n "declare-option str-list ocaml_static_words ${keywords}" | tr '|' ' '

# must be put at last, since we have highlighted some keywords ('type' in 'module type') to other things
  printf %s "
    add-highlighter shared/ocaml/code/ regex \b(${keywords})\b 0:keyword
  "
}

# Conveniences
# ‾‾‾‾‾‾‾‾‾‾‾‾

# C has header and source files and you need to often switch between them.
# Similarly OCaml has .ml (implementation) and .mli (interface files) and
# one often needs to switch between them.
#
# This command provides a simple functionality that allows you to accomplish this.
define-command ocaml-alternative-file -docstring 'Switch between .ml and .mli file or vice versa' %{
    evaluate-commands %sh{
        if [ "${kak_buffile##*.}" = 'ml' ]; then
            printf "edit -- '%s'" "$(printf %s "${kak_buffile}i" | sed "s/'/''/g")"
        elif [ "${kak_buffile##*.}" = 'mli' ]; then
            printf "edit -- '%s'" "$(printf %s "${kak_buffile%i}" | sed "s/'/''/g")"
        fi
    }
}

}

# The OCaml comment is `(* Some comment *)`. Like the C-family this can be a multiline comment.
#
# Recognize when the user is trying to commence a comment when they type `(*` and
# then automatically insert `*)` on behalf of the user. A small convenience.
hook global WinSetOption filetype=ocaml %{
    hook window InsertChar '\*' %{
        try %{
            execute-keys -draft 'HH<a-k>\(\*<ret>'
            execute-keys '  *)<left><left><left>'
        }
    }
}
