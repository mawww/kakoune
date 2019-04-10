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
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ocaml }
}

provide-module ocaml %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ocaml regions
add-highlighter shared/ocaml/code default-region group
add-highlighter shared/ocaml/string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/ocaml/comment region \Q(* \Q*) fill comment

# Macro
# ‾‾‾‾‾

evaluate-commands %sh{
  keywords="and|as|asr|assert|begin|class|constraint|do|done|downto|else|end|exception|external|false"
  keywords="${keywords}|for|fun|function|functor|if|in|include|inherit|initializer|land|lazy|let|lor"
  keywords="${keywords}|lsl|lsr|lxor|match|method|mod|module|mutable|new|nonrec|object|of|open|or"
  keywords="${keywords}|private|rec|sig|struct|then|to|true|try|type|val|virtual|when|while|with"

  printf %s\\n "declare-option str-list ocaml_static_words ${keywords}" | tr '|' ' '

  printf %s "
    add-highlighter shared/ocaml/code/ regex \b(${keywords})\b 0:keyword
  "
}

}
