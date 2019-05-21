# Crystal
# https://crystal-lang.org

hook global BufCreate '.*\.cr' %{
  set-option buffer filetype crystal
}

hook global WinSetOption filetype=crystal %{
  require-module crystal
  add-highlighter window/ ref crystal
  hook -group crystal window InsertChar '\n' crystal-new-line-inserted
  hook -always -once window WinSetOption filetype=.* %{
    remove-highlighter window/crystal
    remove-hooks window crystal
  }
}

provide-module crystal %[
  add-highlighter shared/crystal regions
  add-highlighter shared/crystal/code default-region group
  add-highlighter shared/crystal/code/keywords regex '\b(abstract|alias|annotation|as|asm|begin|break|case|class|def|do|else|elsif|end|ensure|enum|extend|false|for|fun|if|include|instance_sizeof|is_a?|lib|macro|module|next|nil|nil?|of|offsetof|out|pointerof|private|protected|require|rescue|responds_to?|return|select|self|sizeof|struct|super|then|true|type|typeof|uninitialized|union|unless|until|verbatim|when|while|with|yield)\b' 0:keyword
  add-highlighter shared/crystal/string region '"' '(?<!\\)"' regions
  add-highlighter shared/crystal/string/fill default-region fill string
  add-highlighter shared/crystal/string/interpolation region -recurse '\{' '#\{' '\}' fill meta
  add-highlighter shared/crystal/comment region '#' '$' fill comment
  define-command -hidden crystal-new-line-inserted %{
    # Copy previous line indent
    try %(execute-keys -draft 'K<a-&>')
    # Remove empty line indent
    try %(execute-keys -draft 'k<a-x>s^\h+$<ret>d')
  }
  define-command -hidden crystal-fetch-keywords %{
    set-register dquote %sh{
      curl --location https://github.com/crystal-lang/crystal/raw/master/src/compiler/crystal/syntax/lexer.cr |
      kak -f '%1scheck_ident_or_keyword\(:(\w+\??), \w+\)<ret>y%<a-R>a<ret><esc><a-_>a<del><esc>|sort<ret>'
    }
  }
]
