# http://clojure.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require lisp.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-clojure %{
    set buffer filetype clojure
}

hook global BufCreate .*[.](cljs?) %{
    set buffer filetype clojure
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / group clojure

addhl -group /clojure ref lisp

addhl -group /clojure regex \<(clojure.core/['/\w]+)\> 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _clojure_filter_around_selections _lisp_filter_around_selections
def -hidden _clojure_indent_on_new_line       _lisp_indent_on_new_line

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=clojure %[
    addhl ref clojure

    hook window InsertEnd  .* -group clojure-hooks  _clojure_filter_around_selections
    hook window InsertChar \n -group clojure-indent _clojure_indent_on_new_line
]

hook global WinSetOption filetype=(?!clojure).* %{
    rmhl clojure
    rmhooks window clojure-indent
    rmhooks window clojure-hooks
}
