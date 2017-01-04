# http://clojure.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require lisp.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](cljs?) %{
    set buffer filetype clojure
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / group clojure

add-highlighter -group /clojure ref lisp

add-highlighter -group /clojure regex \b(clojure.core/['/\w]+)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _clojure_filter_around_selections _lisp_filter_around_selections
def -hidden _clojure_indent_on_new_line       _lisp_indent_on_new_line

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook -group clojure-highlight global WinSetOption filetype=clojure %{ add-highlighter ref clojure }

hook global WinSetOption filetype=clojure %[
    hook window InsertEnd  .* -group clojure-hooks  _clojure_filter_around_selections
    hook window InsertChar \n -group clojure-indent _clojure_indent_on_new_line
]

hook -group clojure-highlight global WinSetOption filetype=(?!clojure).* %{ remove-highlighter clojure }

hook global WinSetOption filetype=(?!clojure).* %{
    remove-hooks window clojure-indent
    remove-hooks window clojure-hooks
}
