# http://ocaml.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require ocp-indent

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

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden ocaml-indent-on-char %{
  evaluate-commands -no-hooks -draft -itersel %{
    execute-keys ";i<space><esc>Gg|ocp-indent --config base=%opt{indentwidth} --indent-empty --lines %val{cursor_line}<ret>"
  }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group ocaml-highlight global WinSetOption filetype=ocaml %{ add-highlighter window/ocaml ref ocaml }

hook global WinSetOption filetype=ocaml %{
  hook window InsertChar [|\n] -group ocaml-indent ocaml-indent-on-char
}

hook -group ocaml-highlight global WinSetOption filetype=(?!ocaml).* %{ remove-highlighter window/ocaml }

hook global WinSetOption filetype=(?!ocaml).* %{
  remove-hooks window ocaml-indent
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
