# http://clojure.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require lisp.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](cljs?) %{
    set-option buffer filetype clojure
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/clojure group

add-highlighter shared/clojure/ ref lisp

add-highlighter shared/clojure/ regex \b(clojure.core/['/\w]+)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden clojure-filter-around-selections lisp-filter-around-selections
define-command -hidden clojure-indent-on-new-line       lisp-indent-on-new-line

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook -group clojure-highlight global WinSetOption filetype=clojure %{ add-highlighter window/clojure ref clojure }

hook global WinSetOption filetype=clojure %[
    hook window ModeChange insert:.* -group clojure-hooks  clojure-filter-around-selections
    hook window InsertChar \n -group clojure-indent clojure-indent-on-new-line
]

hook -group clojure-highlight global WinSetOption filetype=(?!clojure).* %{ remove-highlighter window/clojure }

hook global WinSetOption filetype=(?!clojure).* %{
    remove-hooks window clojure-indent
    remove-hooks window clojure-hooks
}
