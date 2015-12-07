# http://leiningen.org/grench.html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

def grench -docstring 'eval selection with Grench/Leiningen' %{ %sh{

  if ! grench eval ''; then
    echo "echo 'auto launching headless Leiningen REPL'"
    ( lein repl :headless ) > /dev/null 2>&1 < /dev/null &
    while ! grench eval '' > /dev/null 2>&1; do continue; done
  fi
  }
  info -anchor "%val(cursor_line).%val(cursor_column)" %sh{ grench eval "$kak_selection" }
}

hook global WinSetOption filetype=clojure %{
  map buffer normal <ret> :grench<ret>
}
