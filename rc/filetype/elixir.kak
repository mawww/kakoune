# http://elixir-lang.org
# ----------------------

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ex|exs) %{
    set-option buffer filetype elixir
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=elixir %{
    require-module elixir

    hook window ModeChange pop:insert:.* -group elixir-trim-indent  elixir-trim-indent
    hook window InsertChar \n -group elixir-indent elixir-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window elixir-.+ }
}

hook -group elixir-highlight global WinSetOption filetype=elixir %{
    add-highlighter window/elixir ref elixir
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/elixir }
}


provide-module elixir %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/elixir regions
add-highlighter shared/elixir/code default-region group
add-highlighter shared/elixir/double_string region '"'   (?<!\\)(\\\\)*"   regions
add-highlighter shared/elixir/triple_string region '"""' (?<!\\)(\\\\)*""" ref shared/elixir/double_string
add-highlighter shared/elixir/single_string region "'"   (?<!\\)(\\\\)*'   fill string
add-highlighter shared/elixir/comment       region '#'   '$'               fill comment

add-highlighter shared/elixir/double_string/base default-region fill string
add-highlighter shared/elixir/double_string/interpolation region -recurse \{ \Q#{ \} fill builtin

add-highlighter shared/elixir/code/ regex ':[\w_]+\b' 0:type
add-highlighter shared/elixir/code/ regex '[\w_]+:' 0:type
add-highlighter shared/elixir/code/ regex '[A-Z][\w_]+\b' 0:module
add-highlighter shared/elixir/code/ regex '(:[\w_]+)(\.)' 1:module
add-highlighter shared/elixir/code/ regex '\b_\b' 0:default
add-highlighter shared/elixir/code/ regex '\b_[\w_]+\b' 0:default
add-highlighter shared/elixir/code/ regex '~[a-zA-Z]\(.*\)' 0:string
add-highlighter shared/elixir/code/ regex \b(true|false|nil)\b 0:value
add-highlighter shared/elixir/code/ regex (->|<-|<<|>>|=>) 0:builtin
add-highlighter shared/elixir/code/ regex \b(require|alias|use|import)\b 0:keyword
add-highlighter shared/elixir/code/ regex \b(__MODULE__|__DIR__|__ENV__|__CALLER__)\b 0:value
add-highlighter shared/elixir/code/ regex \b(def|defp|defmacro|defmacrop|defstruct|defmodule|defimpl|defprotocol|defoverridable)\b 0:keyword
add-highlighter shared/elixir/code/ regex \b(fn|do|end|when|case|if|else|unless|var!|for|cond|quote|unquote|receive|with|raise|reraise|try|catch)\b 0:keyword
add-highlighter shared/elixir/code/ regex '@[\w_]+\b' 0:attribute
add-highlighter shared/elixir/code/ regex '\b\d+[\d_]*\b' 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden elixir-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden elixir-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K--\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # indent after line ending with:
	# try %{ execute-keys -draft k x <a-k> (do|else|->)$ <ret> & }
	# filter previous line
        try %{ execute-keys -draft k : elixir-trim-indent <ret> }
        # indent after lines ending with do or ->
        try %{ execute-keys -draft <semicolon> k x <a-k> ^.+(do|->)$ <ret> j <a-gt> }
    }
}

]
