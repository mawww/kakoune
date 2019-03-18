# http://ocaml.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.mli? %{
  set-option buffer filetype ocaml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ocaml regions
add-highlighter shared/ocaml/code default-region group
add-highlighter shared/ocaml/string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/ocaml/comment region \Q(* \Q*) fill comment

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group ocaml-highlight global WinSetOption filetype=ocaml %{
    add-highlighter window/ocaml ref ocaml
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ocaml }
}

# Macro
# ‾‾‾‾‾

evaluate-commands %sh{
  keywords=and:as:asr:assert:begin:class:constraint:do:done:downto:else:end:exception:external:false:for:fun:function:functor:if:in:include:inherit:initializer:land:lazy:let:lor:lsl:lsr:lxor:match:method:mod:module:mutable:new:nonrec:object:of:open:or:private:rec:sig:struct:then:to:true:try:type:val:virtual:when:while:with
  echo "
    add-highlighter shared/ocaml/code/ regex \b($(printf $keywords | tr : '|'))\b 0:keyword
    hook global WinSetOption filetype=ocaml %{
      set-option window static_words $keywords
    }
  "
}
