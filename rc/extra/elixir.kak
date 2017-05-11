# http://elixir-lang.org
# ----------------------

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ex|exs) %{
    set buffer filetype elixir
}


# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code elixir \
    double_string   '"'    (?<!\\)(\\\\)*"         '' \
    double_string   '"""'  (?<!\\)(\\\\)*"""       '' \
    single_string   "'"    (?<!\\)(\\\\)*'         '' \
    comment         '#'    '$'                     ''

add-highlighter -group /elixir/comment fill comment
add-highlighter -group /elixir/double_string  fill string
add-highlighter -group /elixir/double_string regions regions interpolation \Q#{ \} \{
add-highlighter -group /elixir/double_string/regions/interpolation fill builtin
add-highlighter -group /elixir/single_string  fill string
add-highlighter -group /elixir/code regex ':[\w_]+\b' 0:type
add-highlighter -group /elixir/code regex '[\w_]+:' 0:type
add-highlighter -group /elixir/code regex '[A-Z][\w_]+\b' 0:module
add-highlighter -group /elixir/code regex '(:[\w_]+)(\.)' 1:module
add-highlighter -group /elixir/code regex '\b_\b' 0:default
add-highlighter -group /elixir/code regex '\b_[\w_]+\b' 0:default
add-highlighter -group /elixir/code regex '~[a-zA-Z]\(.*\)' 0:string
add-highlighter -group /elixir/code regex \b(true|false|nil)\b 0:value
add-highlighter -group /elixir/code regex (->|<-|<<|>>|=>) 0:builtin
add-highlighter -group /elixir/code regex \b(require|alias|use|import)\b 0:keyword
add-highlighter -group /elixir/code regex \b(__MODULE__|__DIR__|__ENV__|__CALLER__)\b 0:value
add-highlighter -group /elixir/code regex \b(def|defp|defmacro|defmacrop|defstruct|defmodule|defimpl|defprotocol|defoverridable)\b 0:keyword
add-highlighter -group /elixir/code regex \b(fn|do|end|when|case|if|else|unless|var!|for|cond|quote|unquote|receive|with|raise|reraise|try|catch)\b 0:keyword
add-highlighter -group /elixir/code regex '@[\w_]+\b' 0:attribute
add-highlighter -group /elixir/code regex '\b\d+[\d_]*\b' 0:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden elixir-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden elixir-indent-on-new-line %{
    eval -draft -itersel %{
        # copy -- comments prefix and following white spaces 
        try %{ exec -draft k <a-x> s ^\h*\K--\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # indent after line ending with: 
	# try %{ exec -draft k x <a-k> (do|else|->)$ <ret> & }
	# filter previous line
        try %{ exec -draft k : elixir-filter-around-selections <ret> }
        # indent after lines ending with do or ->
        try %{ exec -draft \\; k x <a-k> ^.+(do|->)$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group elixir-highlight global WinSetOption filetype=elixir %{ add-highlighter ref elixir }

hook global WinSetOption filetype=elixir %{
    hook window InsertEnd  .* -group elixir-hooks  elixir-filter-around-selections
    hook window InsertChar \n -group elixir-indent elixir-indent-on-new-line
}

hook -group elixir-highlight global WinSetOption filetype=(?!elixir).* %{ remove-highlighter elixir }

